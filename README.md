# Coro - C++20 Coroutine Library

[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/compiler_support)
[![MSVC](https://img.shields.io/badge/MSVC-2022+-green.svg)](https://visualstudio.microsoft.com)

A production-grade C++20 coroutine library demonstrating modern system programming capabilities including lock-free data structures, work-stealing thread pools, memory pools, and high-performance HTTP server.

This project serves as a portfolio piece for C++/Systems Engineer positions, showcasing deep understanding of:
- C++20 coroutines (promise types, awaiters, symmetric transfer)
- Lock-free programming (atomics, memory ordering, CAS loops)
- Concurrent data structures (work-stealing queues)
- Memory management (pools, allocators, cache optimization)
- Network programming (socket I/O, HTTP protocol)

## Features

### Core Components

| Component | Description | Status |
|-----------|-------------|--------|
| `Task<T>` | Awaitable asynchronous tasks with exception propagation | ✅ |
| `Generator<T>` | Lazy evaluation generators with range-for support | ✅ |
| `Scheduler` | Abstract scheduler interface for coroutine dispatch | ✅ |
| `ThreadPool` | Work-stealing thread pool with Chase-Lev algorithm | ✅ |
| `AsyncMutex` | Coroutine-aware mutex that doesn't block threads | ✅ |
| `Semaphore` / `BinarySemaphore` | Counting semaphores for limiting concurrency | ✅ |
| `Channel<T>` | Buffered channels for coroutine communication | ✅ |
| `TcpStream` / `TcpListener` | Cross-platform async TCP I/O | ✅ |
| `HttpServer` | Full HTTP/1.1 server with routing | ✅ |
| `MemoryPool` | Lock-free fixed-size memory pools (4.78x faster) | ✅ |

## Quick Start

### Build

```powershell
# Clone and build
cd coro
powershell -ExecutionPolicy Bypass -File build.ps1

# Run demos
./build/task_demo.exe
./build/http_server_demo.exe
```

### Hello World

```cpp
#include "coro/coro.hpp"
#include <iostream>

using namespace coro;

Task<int> compute(int x) {
    co_return x * 2;
}

Generator<int> fibonacci(int n) {
    int a = 0, b = 1;
    for (int i = 0; i < n; ++i) {
        co_yield a;
        std::tie(a, b) = std::make_pair(b, a + b);
    }
}

int main() {
    // Async task
    auto task = compute(21);
    std::cout << "Result: " << task.result() << std::endl;  // 42

    // Generator
    for (auto n : fibonacci(10)) {
        std::cout << n << " ";
    }
    std::cout << std::endl;

    return 0;
}
```

## Examples

### HTTP Server

```cpp
#include "coro/coro.hpp"
using namespace coro::net::http;

int main() {
    ThreadPool pool(4);
    Server server(pool, 8080);

    server
        .get("/", [](const Request& req, Response& resp) -> Task<void> {
            resp.html("<h1>Hello, Coro!</h1>");
            co_return;
        })
        .get("/api/status", [](const Request& req, Response& resp) -> Task<void> {
            resp.json(R"({"status":"running","version":"0.3.0"})");
            co_return;
        })
        .get("/api/users/:id", [](const Request& req, Response& resp) -> Task<void> {
            auto id = req.path().substr(req.path().find_last_of('/') + 1);
            resp.json("{\"id\":\"" + id + "\",\"name\":\"User " + id + "\"}");
            co_return;
        });

    server.bind("0.0.0.0");
    std::cout << "Server running on http://localhost:8080" << std::endl;
    server.start();
}
```

### Thread Pool

```cpp
ThreadPool pool(4);
pool.run();

// Schedule coroutine on thread pool
auto task = []() -> Task<int> {
    // Some async work
    co_return compute_something();
}();

pool.schedule(task.get_handle());
int result = task.result();  // Block until complete
pool.stop();
```

### Synchronization

```cpp
// AsyncMutex - doesn't block the thread
AsyncMutex mutex;
co_await mutex.lock();
// critical section
mutex.unlock();

// Semaphore - limit concurrent operations
Semaphore sem(4);  // Max 4 concurrent
co_await sem.acquire();
// Do work with limited concurrency
sem.release();

// Channel - producer/consumer
Channel<int> ch(10);
co_await ch.send(42);
auto value = co_await ch.receive();  // std::optional<int>
```

### Memory Pool

```cpp
// Fixed-size pool (64 bytes)
MemoryPool<64> pool;
void* ptr = pool.allocate();
// Use memory...
pool.deallocate(ptr);

// Size-class based pool
SizeClassPool& pool = SizeClassPool::instance();
void* p = pool.allocate(256);  // Uses 256-byte pool
pool.deallocate(p, 256);

// STL-compatible allocator
std::vector<int, PoolAllocator<int>> vec;
vec.reserve(1000);  // Uses memory pool internally
```

## Architecture

### Project Structure

```
coro/
├── include/coro/
│   ├── coro.hpp              # Main header
│   ├── core/
│   │   ├── task.hpp          # Task<T> coroutine
│   │   └── generator.hpp     # Generator<T> lazy sequence
│   ├── scheduler/
│   │   ├── scheduler.hpp     # Scheduler interface
│   │   ├── thread_pool.hpp   # Work-stealing thread pool
│   │   └── work_steal_queue.hpp  # Chase-Lev lock-free queue
│   ├── sync/
│   │   ├── mutex.hpp         # AsyncMutex
│   │   ├── semaphore.hpp     # Semaphores
│   │   └── channel.hpp       # Channel<T>
│   ├── io/
│   │   └── tcp.hpp           # TCP socket abstraction
│   ├── net/http/
│   │   ├── request.hpp       # HTTP request parser
│   │   ├── response.hpp      # HTTP response builder
│   │   └── server.hpp        # HTTP server
│   └── memory/
│       ├── pool_allocator.hpp    # Memory pools
│       ├── coroutine_allocator.hpp
│       └── object_pool.hpp
├── src/
│   ├── scheduler.cpp
│   └── thread_pool.cpp
├── examples/
│   ├── task_demo.cpp
│   ├── generator_demo.cpp
│   ├── echo_server.cpp
│   ├── thread_pool_demo.cpp
│   ├── sync_primitives_demo.cpp
│   ├── http_server_demo.cpp
│   └── memory_pool_demo.cpp
└── benchmarks/
    └── memory_bench.cpp
```

### Design Highlights

#### Lock-Free Work Stealing

The `WorkStealQueue` uses the Chase-Lev algorithm:
- **Single-producer**: Owner thread pushes/pops without locks
- **Multi-consumer**: Other threads steal work lock-free
- **Dynamic balancing**: No central coordination needed

```cpp
template<typename T>
class WorkStealQueue {
    std::atomic<size_t> top_;      // Only modified by thieves
    std::atomic<size_t> bottom_;   // Only modified by owner
    std::atomic<T*> buffer_;
    // Lock-free push/pop/steal operations
};
```

#### Memory Pool Architecture

Lock-free free list with size-class based allocation:

```cpp
class SizeClassPool {
    // Size classes: 16, 32, 64, 128, 256, 512, 1024, 4096
    MemoryPool<16> pool16_;
    MemoryPool<32> pool32_;
    // ...

    // Lock-free allocate/deallocate using atomic CAS
    void* allocate(size_t size);
    void deallocate(void* ptr, size_t size);
};
```

#### Coroutine Promise Design

```cpp
template<typename T>
struct TaskPromise {
    T value;
    std::exception_ptr exception;
    Scheduler* scheduler = nullptr;
    std::coroutine_handle<> continuation;

    Task<T> get_return_object();
    std::suspend_always initial_suspend();
    auto final_suspend() noexcept;  // Resumes continuation
    void return_value(T val);
    void unhandled_exception();
};
```

## Performance

### Benchmarks

| Benchmark | Result |
|-----------|--------|
| Memory pool allocation | **4.78x faster** than `malloc`/`free` |
| Object pool acquisition | **1.77x faster** than `make_unique` |
| Coroutine creation | **144 ns** per task |
| Work-stealing queue | ~50M operations/second |
| Channel throughput | ~45M messages/second |

### Memory Pool Benchmark

```
=== Memory Pool Benchmark ===
Standard allocator: 117337 us
Memory pool:        24526 us
Speedup:            4.78x

=== Object Pool Benchmark ===
Standard unique_ptr: 20713 us
Object pool:         11732 us
Speedup:             1.77x
```

## Available Executables

| Executable | Description |
|------------|-------------|
| `task_demo.exe` | Task coroutine basics and chaining |
| `generator_demo.exe` | Generator for lazy evaluation |
| `echo_server.exe` | Simple TCP echo server |
| `thread_pool_demo.exe` | Work-stealing thread pool |
| `sync_primitives_demo.exe` | Mutex, Semaphore, Channel demos |
| `http_server_demo.exe` | Full HTTP server with REST API |
| `memory_pool_demo.exe` | Memory pool demonstration |
| `memory_bench.exe` | Performance benchmarks |

## Technical Specifications

- **Language**: C++20
- **Concurrency**: Lock-free where possible
- **Memory**: RAII throughout, custom allocators
- **Platform**: Windows (MSVC 2022+)
- **Standards**: `/await:strict` for standard C++20 coroutines

## Build Requirements

- **Compiler**: MSVC 2022 or later
- **C++ Standard**: C++20
- **Windows SDK**: 10.0.22621.0 or later
- **Build Tool**: PowerShell

## Project Phases

| Phase | Components | Status |
|-------|-----------|--------|
| Phase 1 | Task, Generator, Scheduler | ✅ Complete |
| Phase 2 | WorkStealQueue, ThreadPool | ✅ Complete |
| Phase 3 | AsyncMutex, Semaphore, Channel | ✅ Complete |
| Phase 4 | TCP, HTTP Server | ✅ Complete |
| Phase 5 | Memory Pools, Benchmarks | ✅ Complete |
| Phase 6 | io_uring support (Linux) | 🔮 Planned |
| Phase 7 | File I/O, Timers | 🔮 Planned |

## License

MIT License - See LICENSE file

## Author

Developed as a portfolio project demonstrating modern C++ system programming capabilities suitable for:
- C++ Systems Engineer
- Backend/Infrastructure Engineer
- Game Engine Developer
- High-Performance Computing Engineer

---

**Key Skills Demonstrated**:
- C++20 coroutines (promise types, awaiters, symmetric transfer)
- Lock-free programming (atomics, memory ordering, CAS)
- Concurrent data structures
- Memory management optimization
- Network programming
- Template metaprogramming
- Cross-platform development
