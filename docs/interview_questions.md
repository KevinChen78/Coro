# Coro 项目技术面试题库

本文档整理了针对 Coro C++20 协程库的深度技术面试问题及参考答案，适用于后端工程师/C++系统工程师职位面试。

---

## 目录

1. [C++20 协程基础](#一c20-协程基础)
2. [锁自由编程与并发](#二锁自由编程与并发)
3. [内存管理](#三内存管理)
4. [架构设计](#四架构设计)
5. [网络编程](#五网络编程)
6. [可靠性与测试](#六可靠性与测试)
7. [扩展性](#七扩展性)
8. [性能调优](#八性能调优)

---

## 一、C++20 协程基础

### Q1: final_suspend() 的返回类型设计

**问题**：你的 `Task<T>` 中 `promise_type` 的 `final_suspend()` 为什么返回 `auto` 而不是具体的 awaitable 类型？底层发生了什么？

**参考答案**：

这是为了使用**对称转移（symmetric transfer）**优化。在 C++20 协程中：
- 如果返回 `std::suspend_always`，协程结束时需要额外分配栈帧来调用 continuation
- 如果返回另一个 coroutine_handle，编译器可以进行尾调用优化（TCO），避免栈溢出

```cpp
struct final_awaiter {
    bool await_ready() noexcept { return false; }
    std::coroutine_handle<> await_suspend(std::coroutine_handle<promise_type> h) noexcept {
        // 直接转移控制到 continuation，不增加栈深度
        return h.promise().continuation ? h.promise().continuation
                                        : std::noop_coroutine();
    }
    void await_resume() noexcept {}
};
```

---

### Q2: Generator 异常处理

**问题**：你的 Generator 使用 `co_yield` 实现惰性求值，如果生成器在迭代过程中抛出异常，迭代器状态如何处理？

**参考答案**：

当前实现中，异常会被存储在 promise 中，当迭代器尝试获取下一个值时：
1. 异常会被重新抛出
2. 迭代器变为无效状态（handle 被销毁）
3. 用户需要在 range-for 循环外捕获异常

**改进方案**：
```cpp
class GeneratorIterator {
public:
    reference operator*() {
        if (handle_.promise().exception_) {
            std::rethrow_exception(handle_.promise().exception_);
        }
        return *handle_.promise().value_ptr_;
    }
};
```

---

## 二、锁自由编程与并发

### Q3: WorkStealQueue 内存序选择

**问题**：你的 WorkStealQueue 使用 Chase-Lev 算法，请解释为什么 `steal()` 操作使用 `memory_order_seq_cst` 而 `pop()` 使用 `memory_order_acquire`？如果都用 `acquire/release` 会怎样？

**参考答案**：

这是 **ABA 问题** 和 **同步顺序** 的关键：

```cpp
// steal() - 多线程竞争
std::atomic_thread_fence(std::memory_order_seq_cst);  // 全局同步点

// pop() - 单线程操作
size_t t = top_.load(std::memory_order_acquire);  // 只需要看到 thief 的修改
```

**风险**：如果都用 `acquire/release`：
1. **ABA 问题**：Thief A 看到 top=5，被抢占，Owner pop 使 top=6 然后 push 使 top=5，Thief A 继续执行 CAS 成功，但数据已变
2. **顺序不一致**：不同线程看到的修改顺序可能不同，导致 race condition

`seq_cst` 确保所有线程对操作顺序有一致的视图。

---

### Q4: MemoryPool 活锁风险

**问题**：在 `MemoryPool` 的 `deallocate` 中，你的 CAS 循环可能导致活锁（livelock）吗？高并发下的性能表现如何？

**参考答案**：

**活锁风险**：极低，因为 CAS 失败会更新 `block->next`，每次迭代都取得最新值。

**性能分析**：
- 无竞争：1 次 CAS 成功
- 低竞争：2-3 次重试
- 高竞争：可能指数退避

**优化方案**：
```cpp
// 指数退避避免缓存风暴
for (int retries = 0; ; ++retries) {
    if (free_list_.compare_exchange_weak(...)) break;

    if (retries < 4) {
        _mm_pause();  // 告诉 CPU 我们在自旋
    } else {
        std::this_thread::yield();  // 让出时间片
    }
}
```

---

## 三、内存管理

### Q5: SizeClassPool 尺寸选择

**问题**：`SizeClassPool` 有 8 个固定大小的内存池，为什么选择这些特定尺寸（16,32,64...4096）？如果分配 24 字节会发生什么？

**参考答案**：

**选择依据**：
- 16-64：小对象，常见的小容器、指针
- 128-512：中等对象，字符串、小型缓冲区
- 1024-4096：大对象，协程帧、网络缓冲区

**24 字节分配**：
向上取整到 32 字节池，产生 **8 字节内部碎片（33% 碎片率）**

**改进策略**：
```cpp
// 更细粒度的 size classes
// 16, 24, 32, 48, 64, 96, 128, 192, 256...
// 但会增加管理开销，需要权衡
```

---

### Q6: PoolAllocator 与 STL 扩容

**问题**：你的 `PoolAllocator` 如何与 STL 容器配合？如果 `std::vector` 使用你的 allocator 后扩容，内存池会如何处理？

**参考答案**：

```cpp
std::vector<int, PoolAllocator<int>> vec;
vec.reserve(100);  // 分配 400 字节 -> 512 字节池

// 扩容时
vec.resize(200);   // 需要 800 字节，超过 4096 限制
// 回退到 ::operator new
```

**问题**：分配和释放必须使用相同的 allocator，且 size 必须匹配。

**解决方案**：
```cpp
// 跟踪实际分配大小
struct AllocHeader {
    size_t size;
};

void* allocate(size_t n) {
    size_t bytes = n * sizeof(T) + sizeof(AllocHeader);
    void* ptr = SizeClassPool::instance().allocate(bytes);
    static_cast<AllocHeader*>(ptr)->size = bytes;
    return static_cast<char*>(ptr) + sizeof(AllocHeader);
}

void deallocate(void* ptr, size_t n) {
    char* real_ptr = static_cast<char*>(ptr) - sizeof(AllocHeader);
    size_t bytes = static_cast<AllocHeader*>(real_ptr)->size;
    SizeClassPool::instance().deallocate(real_ptr, bytes);
}
```

---

## 四、架构设计

### Q7: ThreadPool 外部队列瓶颈

**问题**：你的 `ThreadPool` 使用 `std::mutex` 和 `std::condition_variable` 实现外部任务队列，这会成为瓶颈吗？如何优化？

**参考答案**：

**瓶颈分析**：
- 高并发提交时，多个线程竞争 `external_mutex_`
- 每批次 steal 只拿一个任务，效率低

**优化方案**：

1. **无锁外部队列**：
```cpp
class LockFreeQueue {
    std::atomic<Node*> head_;
    std::atomic<Node*> tail_;
    // Michael-Scott 队列算法
};
```

2. **批量任务提交**：
```cpp
void schedule_batch(std::vector<std::coroutine_handle<>> handles) {
    std::lock_guard<std::mutex> lock(external_mutex_);
    external_queue_.insert(external_queue_.end(),
                          handles.begin(), handles.end());
}
```

3. **工作线程批量偷取**：
```cpp
// 偷取一半而非一个
size_t steal_count = (b - t) / 2;
for (size_t i = 0; i < steal_count; ++i) {
    stolen.push_back(steal_one());
}
```

---

## 五、网络编程

### Q8: HTTP 服务器 C10K 问题

**问题**：你的 HTTP 服务器每个连接开一个 `std::thread`，这在高并发下（如 10k 连接）会有什么问题？如何改进？

**参考答案**：

**问题**：
- 10k 线程 = 10k 栈空间（默认 8MB）= 80GB 虚拟内存
- 上下文切换开销巨大
- 线程创建/销毁开销高

**改进方案**：

1. **线程池 + 事件循环**（Reactor 模式）：
```cpp
class HttpServer {
    ThreadPool io_pool_{std::thread::hardware_concurrency()};

    void start() {
        for (auto& worker : io_pool_) {
            worker.run([this]() {
                io_uring_loop();  // Linux
                // or IOCPGetQueuedCompletionStatus() // Windows
            });
        }
    }
};
```

2. **io_uring（Linux）**：
```cpp
// 真正的异步 I/O，不需要线程每连接
struct io_uring_sqe* sqe = io_uring_get_sqe(&ring);
io_uring_prep_read(sqe, fd, buf, len, 0);
io_uring_sqe_set_data(sqe, coroutine_handle);
io_uring_submit(&ring);
```

---

### Q9: HTTP 解析器性能

**问题**：HTTP 解析器使用字符串操作，如 `substr` 和 `find`，这存在哪些性能隐患？如何优化？

**参考答案**：

**性能问题**：
- `substr` 可能分配新内存
- `find` 线性扫描，O(n) 复杂度
- 多次字符串拷贝

**零拷贝优化**：
```cpp
class Request {
    std::string_view raw_data_;  // 引用原始缓冲区，不拷贝

    struct Header {
        std::string_view name;   // 指向 raw_data_ 内部
        std::string_view value;
    };

    // 解析时不分配，只记录偏移量
    bool parse(std::string_view data) {
        raw_data_ = data;
        auto line_end = data.find("\r\n", pos);
        method_ = data.substr(pos, space_pos - pos);
        // ...
    }
};
```

---

## 六、可靠性与测试

### Q10: Channel 线程安全

**问题**：你的 Channel 是单生产者单消费者还是多生产者多消费者？如果是多生产者，有什么线程安全问题？

**参考答案**：

当前实现使用 `std::mutex` 保护，是**多生产者多消费者**的，但受限于锁。

**潜在问题**：
```cpp
if (buffer_.size() < capacity_) {  // 检查
    buffer_.push(std::move(value)); // 操作
    co_return true;
}
```
这不是原子操作，需要锁保护。

**无锁 Channel 实现**：
```cpp
template<typename T>
class LockFreeChannel {
    struct Node {
        std::atomic<T*> data_;
        std::atomic<Node*> next_;
    };

    std::atomic<Node*> head_;
    std::atomic<Node*> tail_;
    std::atomic<size_t> size_;
};
```

---

### Q11: 协程帧分配失败处理

**问题**：如果 `Task` 的 `promise` 在协程执行过程中抛出 `bad_alloc`，当前实现如何处理？

**参考答案**：

当前实现：
```cpp
void unhandled_exception() {
    exception_ = std::current_exception();
}
```

这会捕获异常，但**协程帧分配失败**发生在协程创建前，无法进入 `unhandled_exception`。

**改进**：
```cpp
// 自定义 operator new
void* operator new(size_t size) {
    auto* ptr = CoroutineFramePool::instance().allocate(size);
    if (!ptr) throw std::bad_alloc();
    return ptr;
}

// 或者在 allocate 中处理
void* allocate_coroutine_frame(size_t size) {
    void* ptr = pool.allocate(size);
    if (!ptr) {
        // 降级到系统分配器
        ptr = ::operator new(size);
    }
    return ptr;
}
```

---

## 七、扩展性

### Q12: 协程超时功能设计

**问题**：如果要添加协程超时功能，你会如何设计？需要修改哪些组件？

**参考答案**：

**设计方案**：

1. **超时 Awaiter**：
```cpp
template<typename Rep, typename Period>
Task<void> sleep_for(std::chrono::duration<Rep, Period> timeout) {
    co_await SleepAwaiter{timeout};
}

struct SleepAwaiter {
    std::chrono::milliseconds duration;

    bool await_ready() { return duration.count() <= 0; }
    void await_suspend(std::coroutine_handle<> h) {
        TimerQueue::instance().schedule(duration, h);
    }
    void await_resume() {}
};
```

2. **带超时的操作**：
```cpp
template<typename T>
Task<std::optional<T>> with_timeout(Task<T> task,
                                     std::chrono::milliseconds timeout) {
    auto timeout_task = []() -> Task<void> {
        co_await sleep_for(timeout);
    }();

    auto result = co_await race(task, timeout_task);
    if (result.index() == 1) {
        co_return std::nullopt;  // 超时
    }
    co_return std::get<0>(result);
}
```

**需要修改**：
- 添加 `TimerQueue` 组件（优先级队列 + 事件循环）
- 修改 Scheduler 支持定时唤醒
- 添加 `race` 组合子

---

## 八、性能调优

### Q13: 协程创建性能优化

**问题**：你的 benchmark 显示协程创建需要 144ns，这个性能在同行业什么水平？如何进一步优化到 100ns 以下？

**参考答案**：

**行业对比**：
- Goroutine：~200ns（有栈协程，包含初始栈）
- Rust async/await：~50-100ns
- C++ std::thread：~10μs
- **144ns 处于较好水平**

**优化到 100ns 以下**：

1. **预分配协程帧池**：
```cpp
class CoroutinePool {
    std::array<char, 64*1024> buffer_;  // 预分配 64KB
    size_t offset_ = 0;

    void* allocate(size_t size) {
        if (offset_ + size <= buffer_.size()) {
            void* ptr = &buffer_[offset_];
            offset_ += (size + 15) & ~15;  // 16 字节对齐
            return ptr;
        }
        return fallback_allocator.allocate(size);
    }
};
```

2. **内联小协程**：
```cpp
// 对于简单协程，使用无栈帧优化
Task<int> simple() {
    // 编译器可能内联整个协程体
    co_return 42;
}
```

3. **批量创建**：
```cpp
std::vector<Task<int>> create_tasks() {
    std::vector<Task<int>> tasks;
    tasks.reserve(1000);
    for (int i = 0; i < 1000; ++i) {
        tasks.push_back(make_task(i));  // 批量分配
    }
    return tasks;
}
```

---

## 评分标准

| 能力维度 | 考察重点 | 优秀回答特征 |
|---------|---------|-------------|
| 协程原理 | symmetric transfer、promise 设计 | 能解释 TCO 优化和栈溢出避免 |
| 并发编程 | 内存序、ABA 问题、活锁 | 能说出 seq_cst 的必要性 |
| 内存管理 | size class、碎片、allocator | 能提出跟踪实际分配大小的方案 |
| 架构设计 | 瓶颈分析、扩展性 | 能识别 mutex 瓶颈并提出无锁方案 |
| 网络编程 | C10K、零拷贝、io_uring | 了解事件驱动和异步 I/O |
| 可靠性 | 异常安全、线程安全 | 能识别析构和异常边界问题 |
| 工程经验 | 性能优化、调试技巧 | 有实际优化经验和工具使用 |

---

## 综合评分参考

| 分数 | 评价 |
|-----|------|
| 9-10 | 资深专家，能设计并实现工业级协程库 |
| 7-8 | 高级工程师，理解原理并有优化能力 |
| 5-6 | 中级工程师，能用但理解不够深入 |
| <5 | 初级水平，需要加强底层原理学习 |

**本项目综合评分：8.5/10**（高级后端工程师水平）
