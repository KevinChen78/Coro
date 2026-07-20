# coro — 火焰图调优审计（run-1）

**日期**：2026-06-30 · **工具链**：WSL2 Ubuntu + Linux perf 6.8.12 + Brendan Gregg FlameGraph
**构建**：`g++ -std=c++20 -fcoroutines -O2 -g -fno-omit-frame-pointer`（build-prof/，GCC-12）
**采样**：`perf record -F 999 -g --call-graph=dwarf -e task-clock:u` × 10 轮，通过 `stackcollapse-perf.pl` 合并

---

## TL;DR

| 场景 | 综合分 | 评级 | 头条结论 |
|---|---|---|---|
| HTTP 服务器（c=32 -k） | **63.3** | C | `pthread_create` 占 **30.66%**——服务器没用本库自己的协程 |
| 内存池（memory_bench） | **67.1** | C | 内存池本身给出 **4.30×**，README 声称 4.78×（偏差 10%），但基准被 *malloc 对比基线* 主导，而非池本身 |

库的结构是健全的（无锁池工作正常，原子 CAS 在正确的路径上），但 **招牌 HTTP 示例不吃自己的狗粮** —— 它对每个连接用 `std::thread::detach`，而不是用 `coro::Task` 调度。

---

## 场景 A —— HTTP 服务器（`coro_http_server`）

**负载**：`webbench -c 32 -t 28 -k http://localhost:8080/` → **28,877 QPS**，808,572 成功 / 2,131 失败。

### Top-5 自身热点（占 CPU 74.57%）

| % | 函数 | 解读 |
|---|---|---|
| **30.66%** | `pthread_create` | 每连接创建线程 |
| 21.60% | `[libc.so.6]` | TLS + 动态链接器胶水 |
| 9.76% | `[ld-linux-x86-64.so.2]` | `_dl_allocate_tls_init` |
| 7.67% | `malloc` | 线程栈分配 |
| 4.88% | `_dl_allocate_tls_init` | 每线程 TLS 初始化 |

### 矛盾之处

这是一个 README 头条写着如下内容的库：

> C++20 coroutines（promise types、awaiters、symmetric transfer）
> ……
> | `HttpServer` | Full HTTP/1.1 server with routing | ✅ |

……然而它自己的 HTTP 服务器请求循环却是 `accept → std::thread(&handler, conn) → detach`。库的 `Task<T>`、`ThreadPool`、`EventLoop`、`TcpAcceptor` 全都定义了——火焰图也显示 `coro::io::TcpAcceptor::accept`（1.74%）在路径上——但真正的请求处理跑在崭新的 `std::thread` 上，而不是由库自己的池调度的协程上。

[`coro_http/flamegraph.svg`](../../webserver/profiling/run-1/coro_http/flamegraph.svg) 里的 `[accept](…) → [std::thread::_M_start_thread](…)` 链就是证据。

### 结论

HTTP 服务器是本次审计里最大的调优失败——不是 28K QPS 慢（并不慢），而是瓶颈（30% 的 `pthread_create`）**正是这个协程库本应消除的开销**。面试官跑一下这服务器再去读源码，30 秒就能发现这个不一致。

### KPI 分项（综合分 63.3，评级 C）

| KPI | 得分 | 原始值 | 备注 |
|---|---|---|---|
| K1 集中度 | 41.7 | top-5 = 74.6% | 全是基础设施，看不到应用逻辑 |
| K2 分配 | 0 | 11.5% | 线程栈 |
| K3 锁 | 100 | 4.5% | spawn 簿记周围的 mutex |
| K4 原子 | 100 | 0% | —— |
| K5 系统调用 | 100 | 6.3% | accept() 是合理的 |
| K6 空闲/等待 | 100 | 0% | CPU 密集型 |
| K7 memcpy | 100 | 0% | —— |
| K8 叶子多样性 | 40 | 11 个叶子 | 扁平轮廓（一个主要开销） |
| K9 调用栈深度 | 100 | p90 = 10 | 浅得合理 |
| K10 README 复现 | 0 | 偏差 44.5% | 实测 28.8K vs 同类服务隐含的 ~52K |

---

## 场景 B —— 内存池（`memory_bench`）

**基准形态**：1M 次 `allocate(64)` 然后 1M 次 `deallocate`，重复 5×，对比 `malloc` vs `MemoryPool<64>` vs `unique_ptr` vs `ObjectPool`。外加 work-stealing 队列微基准。

### 实测 vs README

| 工作负载 | 标准 | 池 | 加速比 | README 声称 | 结论 |
|---|---|---|---|---|---|
| `MemoryPool<64>` vs `malloc` | 82,487 µs | 19,188 µs | **4.30×** | 4.78× | **−10%，准确** ✅ |
| `ObjectPool<TestObject>` vs `unique_ptr` | 14,537 µs | 7,619 µs | **1.91×** | （未声称） | —— |

K10 = 99.8（偏差 10% 以内）。池的头条数字是真实的。

### Top-5 自身热点（占 CPU 70.4%）

| % | 函数 | 解读 |
|---|---|---|
| 44.35% | `[libc.so.6]` | 主要是基准测试里 **对比基线** 部分的 `malloc`/`cfree` |
| 9.40% | `malloc` | 基线分配器路径 |
| 7.38% | `:1164234`（未解析） | perf 无法符号化——很可能是内联的池辅助函数 |
| 5.07% | `cfree` | 基线 free |
| **4.19%** | `MemoryPool<64>::FreeBlock::compare_exchange_weak` | **这才是池本身的热点 CAS** |

### 如何正确解读这份数据

44% 的 `[libc.so.6]` 看着吓人，但 **是预期的** ——每次基准迭代有一半在调原始 `malloc`/`free` 作为对比基线。池自身的开销是 ~4% CAS + ~3% `allocate` + ~1% `deallocate` ≈ **总量的 ~8%**，在同样硬件上比 malloc 的 ~15%（malloc + cfree + libc）快约 2×。这就是 4.30× 端到端加速比的来源。

### 池本身可以改进的地方

- `compare_exchange_weak` 占 4.19% 是无锁 free-list 头部的 CAS。争用下这个 CAS 会重试。合理的优化是 per-thread slab + 共享中央 free-list（mimalloc 风格），用一些内存换更少的争用。
- `WorkStealQueue::resize` 占 1.89% 值得警惕——resize 应该很少发生；如果它在热路径上，说明队列的初始容量对该工作负载太小。
- 池基准里出现 `operator new`（2.92%）意味着池自己在某处调了 `::operator new`——很可能是底层 slab。可以接受，但值得确认是摊销的。

### KPI 分项（综合分 67.1，评级 C）

| KPI | 得分 | 原始值 | 备注 |
|---|---|---|---|
| K1 集中度 | 58.4 | top-5 = 70.4% | 由 malloc 基线主导 |
| K2 分配 | 0 | 18.6% | 被基准 harness 里的基线 `malloc`/`free` 抬高——对池不公平 |
| K3 锁 | 100 | 1.6% | 池确实是无锁的 |
| K4 原子 | 0 | 9.6% | `FreeBlock` 的 CAS + 计数器的 atomic load/store——这是无锁设计的 **预期代价** |
| K5 系统调用 | 100 | 0% | 纯用户态 |
| K6 空闲/等待 | 100 | 0% | CPU 密集型 |
| K7 memcpy | 100 | 0% | —— |
| K8 叶子多样性 | 70 | 15 个叶子 | 合理 |
| K9 调用栈深度 | 73.3 | p90 = 19 | 健康 |
| K10 README 复现 | 99.8 | 偏差 10% | **README 是诚实的** ✅ |

### K2/K4 注意事项

K2（分配）和 K4（原子）都得 0——但高分配 % 很大程度上是基准里 *对比基线* 的 `malloc` 跑出来的，不是池；高原子 % 是 *有意为之* 的 CAS 设计（"无锁"的核心）。如果基准只测池本身（或对比 Fraser/Harris 无锁基线，而非原始 malloc），这两项 KPI 都会更高。评级 C 部分是评分规则的伪影。

---

## 最优先修复（按 ROI 排序）

1. **HTTP：把 `std::thread::detach` 换成 `co_await` + `coro::ThreadPool::schedule`。** 这是对库可信度伤害最大的发现。池、task、acceptor 都存在——把它们接起来。
2. **MemoryPool：per-thread slab。** 把单一的共享 free-list 头（CAS 争用 4.19%）拆成 per-thread + 中央。应该能把 4.30× 加速比在争用下推到或超过 4.78× 声称值。
3. **WorkStealQueue：调查 `resize`（1.89%）。** 一个在正常运行期间 resize 的 work-stealing 队列存在容量 bug。

## 已经做得好的地方

- `MemoryPool` 的无锁设计正确，README 的 4.78× 可复现（偏差 10% 以内）。
- HTTP 服务器的 accept 循环正确使用了 `coro::io::TcpAcceptor::accept`——只有 handler 派发错了。
- 池路径本身没有 futex/mutex 争用（K3 = 100）。
- 调用栈深度健康（p90 ≤ 19）——没有模板实例化爆炸。

## 产物

```
coro/profiling/run-1/
├── REPORT.md                         (本文件)
├── coro_mem/
│   ├── flamegraph.svg                ← 浏览器打开
│   ├── folded.txt
│   ├── top-self-merged.txt
│   ├── top-self-single.txt           ← 单轮 perf report
│   └── scores.json                   ← 综合分 67.1
└── (HTTP 场景产物位于 webserver/profiling/run-1/coro_http/
     因为负载生成器 webbench 是从 webserver 构建的)
```

HTTP 场景产物位于 [`../../webserver/profiling/run-1/coro_http/`](../../webserver/profiling/run-1/coro_http/) ——服务器二进制是 `coro_http_server`（coro 项目），但负载生成器 `webbench` 是从 webserver 项目构建的，所以运行目录最终落在 webserver/ 下。数据归属本报告。

## 跨项目说明

参见 [`../../webserver/profiling/run-1/REPORT.md`](../../webserver/profiling/run-1/REPORT.md) 中对 webserver 代码库的对应审计。HTTP 的发现 **完全一致**，因为同一个 `coro_http_server` 二进制是两个项目事实上的 HTTP 栈（webserver 没有自己的 HTTP 服务器——见该报告的架构发现一节）。
