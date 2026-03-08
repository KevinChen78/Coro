# Coro Architecture Documentation

This document describes the internal architecture and design decisions of the Coro library.

## Overview

Coro is a C++20 coroutine library built with performance and modern C++ practices in mind. The architecture follows these principles:

1. **Zero-cost abstractions**: Features should not impose runtime overhead if not used
2. **Lock-free first**: Prefer lock-free algorithms for hot paths
3. **RAII everywhere**: No raw allocations, automatic resource management
4. **Composability**: Components work together seamlessly

## Core Components

### 1. Task<T> - Asynchronous Tasks

The `Task<T>` type represents a coroutine that produces a value of type T.

#### Promise Type

```cpp
template<typename T>
struct TaskPromise {
    // Storage
    alignas(T) char value_buffer_[sizeof(T)];
    std::exception_ptr exception_;

    // Continuation support
    Scheduler* scheduler_ = nullptr;
    std::coroutine_handle<> continuation_;

    // Lifecycle
    Task<T> get_return_object();
    std::suspend_always initial_suspend() { return {}; }
    auto final_suspend() noexcept;

    // Result handling
    template<typename U>
    void return_value(U&& value);
    void unhandled_exception();

    // Awaiter interface
    TaskAwaiter<T> await_transform(Task<T>&& task);
};
```

#### Awaiter Pattern

```cpp
template<typename T>
struct TaskAwaiter {
    bool await_ready() const noexcept;
    void await_suspend(std::coroutine_handle<> handle);
    T await_resume();
};
```

**Key Design Decisions**:
- Lazy execution: Task doesn't start until awaited
- Automatic scheduler propagation: Child tasks inherit parent's scheduler
- Exception safety: Exceptions are captured and rethrown on `result()`

### 2. Generator<T> - Lazy Sequences

The `Generator<T>` type produces a sequence of values on demand.

```cpp
template<typename T>
class Generator {
    struct Promise {
        T* current_value_;
        // ...
    };

public:
    class Iterator {
        std::coroutine_handle<Promise> handle_;
        // Input iterator interface
    };

    Iterator begin();
    Iterator end();
};
```

**Key Design Decisions**:
- Iterator-based for range-for compatibility
- Single-pass (input iterator) for simplicity
- Lazy evaluation: Values computed only when requested

### 3. Scheduler - Execution Context

The `Scheduler` interface abstracts coroutine dispatch:

```cpp
class Scheduler {
public:
    virtual void schedule(std::coroutine_handle<> handle) = 0;
    virtual void run() = 0;
    virtual void stop() = 0;
    virtual bool is_on_scheduler_thread() const = 0;
};
```

Implementations:
- `InlineScheduler`: Executes immediately (for testing)
- `ThreadPool`: Work-stealing thread pool

## Scheduler Implementation

### Work Stealing Algorithm

The `WorkStealQueue` implements the Chase-Lev algorithm:

#### Algorithm Overview

1. **Owner Thread** (push/pop):
   - Push: Increment bottom, write to array
   - Pop: Read from array, decrement bottom

2. **Thief Threads** (steal):
   - Read top, CAS increment if successful

#### Data Layout

```
Array (circular buffer):
[ ][ ][ ][T][ ][ ][B][ ][ ]
           ^     ^
           top   bottom

Owner works from bottom up
Thieves steal from top down
```

#### Implementation Details

```cpp
template<typename T>
class WorkStealQueue {
    std::atomic<size_t> top_;
    std::atomic<size_t> bottom_;
    std::atomic<T*> buffer_;
    std::atomic<size_t> capacity_;

    // Owner operations (lock-free)
    void push(T item);
    std::optional<T> pop();

    // Thief operation (lock-free)
    std::optional<T> steal();

    // Grow array when full
    void grow();
};
```

**Memory Ordering**:
- `push`: `memory_order_release` on bottom, `memory_order_relaxed` on data
- `pop`: `memory_order_acquire` on top (for synchronization with thieves)
- `steal`: `memory_order_seq_cst` for global synchronization

### Thread Pool

```cpp
class ThreadPool : public Scheduler {
    struct Worker {
        WorkStealQueue<std::coroutine_handle<>> local_queue;
        std::atomic<size_t> steal_attempts{0};
        std::atomic<bool> running{true};
        std::mutex mutex;
        std::condition_variable cv;
    };

    std::vector<std::unique_ptr<Worker>> workers_;
    std::vector<std::thread> threads_;

    void worker_loop(size_t worker_id);
    std::optional<std::coroutine_handle<>> get_task(size_t worker_id);
    bool steal_task(size_t thief_id, std::coroutine_handle<>& handle);
};
```

**Work Distribution**:
1. Try local queue first (LIFO - good for cache locality)
2. Try stealing from other threads (FIFO - good for load balancing)
3. Steal from random victim to reduce contention

## Synchronization Primitives

### AsyncMutex

Traditional mutex blocks the thread. `AsyncMutex` suspends the coroutine instead:

```cpp
class AsyncMutex {
    class LockAwaiter {
        AsyncMutex& mutex_;
        std::coroutine_handle<> handle_;

        bool await_ready();
        void await_suspend(std::coroutine_handle<> handle);
        void await_resume();
    };

    std::atomic<bool> locked_{false};
    std::queue<std::coroutine_handle<>> waiters_;

public:
    LockAwaiter lock();
    void unlock();
};
```

**Algorithm**:
1. Try CAS acquire
2. If failed, enqueue waiter and suspend
3. On unlock, dequeue and resume first waiter (or release lock)

### Semaphore

```cpp
class CountingSemaphore {
    std::atomic<std::ptrdiff_t> count_;
    std::mutex mutex_;
    std::queue<std::pair<std::coroutine_handle<>, std::ptrdiff_t>> waiters_;

public:
    AcquireAwaiter acquire(std::ptrdiff_t n = 1);
    bool try_acquire(std::ptrdiff_t n = 1);
    void release(std::ptrdiff_t n = 1);
};
```

### Channel<T>

Buffered channel for producer-consumer communication:

```cpp
template<typename T>
class Channel {
    std::mutex mutex_;
    std::queue<T> buffer_;
    size_t capacity_;
    std::atomic<bool> closed_{false};

public:
    Task<bool> send(T value);  // Returns false if closed
    Task<std::optional<T>> receive();  // Returns nullopt if closed/empty
};
```

## Memory Management

### Memory Pool

Fixed-size memory pools with lock-free allocation:

```cpp
template<size_t BlockSize, size_t BlocksPerChunk = 64>
class MemoryPool {
    struct FreeBlock {
        FreeBlock* next;
    };

    std::atomic<FreeBlock*> free_list_;
    Chunk* chunk_list_;
    std::mutex mutex_;  // Only for chunk allocation

public:
    void* allocate();
    void deallocate(void* ptr);
};
```

**Allocation Path**:
1. Try lock-free pop from free list (hot path)
2. If empty, allocate from current chunk under lock
3. If chunk full, allocate new chunk under lock
4. Add remaining blocks to free list

**Deallocation Path**:
1. Push to free list using CAS loop (lock-free)

### Size-Class Based Allocation

```cpp
class SizeClassPool {
    MemoryPool<16> pool16_;
    MemoryPool<32> pool32_;
    // ... 64, 128, 256, 512, 1024, 4096

public:
    void* allocate(size_t size);
    void deallocate(void* ptr, size_t size);
};
```

Size classes are chosen to minimize internal fragmentation while limiting the number of pools.

### Coroutine Frame Allocation

The `CORO_FRAME_ALLOCATOR` macro enables pooled allocation for coroutine frames:

```cpp
struct Promise {
    CORO_FRAME_ALLOCATOR  // Adds operator new/delete
    // ...
};
```

This reduces allocation overhead for frequently created coroutines.

## Network I/O

### TCP Abstraction

The TCP layer provides coroutine-friendly I/O operations:

```cpp
class TcpStream {
    Socket socket_;

public:
    Task<std::optional<size_t>> read(std::vector<uint8_t>& buffer);
    Task<std::optional<size_t>> write(const uint8_t* data, size_t len);
    Task<std::optional<std::string>> read_line();
};
```

**Design Decisions**:
- Blocking I/O with timeouts (simpler than async I/O)
- Each connection handled by separate thread (for now)
- Future: io_uring for true async I/O on Linux

### HTTP Server

```cpp
class Server {
    ThreadPool& pool_;
    TcpListener listener_;
    Router router_;

    Task<void> handle_connection(TcpStream stream);

public:
    void start();
    void stop();

    // Routing
    Server& get(const std::string& path, Handler handler);
    Server& post(const std::string& path, Handler handler);
    // ...
};
```

Request handling flow:
1. Accept connection in main thread
2. Spawn detached thread for each connection
3. Parse HTTP request
4. Route to handler
5. Send response
6. Close connection (or keep-alive)

## Performance Optimizations

### 1. Cache Locality

- Work-stealing queue: Owner uses LIFO (cache hot), thieves use FIFO (cache cold)
- Memory pools: Objects reused from same memory location
- Thread-local storage: Worker ID cached per thread

### 2. Lock-Free Hot Paths

- Memory allocation: Free list operations are lock-free
- Work stealing: Steal operation is lock-free
- Task scheduling: Local queue push/pop is lock-free

### 3. False Sharing Prevention

```cpp
struct alignas(64) Worker {
    // Worker data padded to cache line size
};
```

### 4. Batch Operations

- Work stealing: Steal half the queue to amortize cost
- Memory pools: Allocate chunks in batches

## Error Handling

### Exception Safety

All components provide strong exception safety guarantees:

- **Task**: Exceptions captured in promise, rethrown on `result()`
- **Generator**: Iterator invalidated on exception
- **Channel**: Close operation safe even during operations
- **Memory pools**: Allocation failures fall back to system allocator

### Cancellation

Not currently implemented. Future work:
- Cancellation tokens
- Cooperative cancellation points
- Resource cleanup on cancellation

## Testing Strategy

### Unit Tests

- Component isolation: Each component tested independently
- Mock schedulers: InlineScheduler for deterministic testing
- Edge cases: Empty sequences, exceptions, concurrent access

### Integration Tests

- End-to-end workflows: Producer-consumer patterns
- Stress tests: High contention scenarios
- Performance tests: Benchmarks with statistical analysis

## Future Directions

### io_uring Support (Linux)

True async I/O without blocking threads:

```cpp
class IoUringScheduler : public Scheduler {
    io_uring ring_;
    // Submit operations to ring
    // Poll for completions
};
```

### Zero-Copy Networking

- Buffer pools for network I/O
- Scatter-gather I/O operations
- Direct buffer recycling

### Timers

```cpp
Task<void> sleep(std::chrono::milliseconds duration);
```

Implementation options:
- Timer wheel
- Priority queue with epoll/kqueue/IOCP timeout

### Coroutine Local Storage

Thread-local storage equivalent for coroutines:

```cpp
template<typename T>
class CoroLocal {
    T& get();
    void set(T value);
};
```

## References

- [C++ Coroutines TS](https://en.cppreference.com/w/cpp/language/coroutines)
- [Chase-Lev Work Stealing](https://dl.acm.org/doi/10.1145/324133.324234)
- [Lewis Baker's Coroutine Series](https://lewissbaker.github.io/)
- [cppcoro Library](https://github.com/lewissbaker/cppcoro)
