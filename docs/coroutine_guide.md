# Coro User Guide

This guide covers common usage patterns and best practices for the Coro library.

## Table of Contents

1. [Getting Started](#getting-started)
2. [Task Patterns](#task-patterns)
3. [Generator Patterns](#generator-patterns)
4. [Thread Pool Usage](#thread-pool-usage)
5. [Synchronization](#synchronization)
6. [Memory Management](#memory-management)
7. [HTTP Server](#http-server)
8. [Best Practices](#best-practices)
9. [Common Pitfalls](#common-pitfalls)

## Getting Started

### Include the Library

```cpp
#include "coro/coro.hpp"
using namespace coro;
```

### Your First Coroutine

```cpp
#include "coro/coro.hpp"
#include <iostream>

Task<int> compute_sum(int a, int b) {
    co_return a + b;
}

int main() {
    auto task = compute_sum(10, 20);
    int result = task.result();  // Blocks until complete
    std::cout << "Sum: " << result << std::endl;  // 30
    return 0;
}
```

## Task Patterns

### Basic Async Operation

```cpp
Task<std::string> fetch_user_name(int user_id) {
    // Simulate async database query
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    co_return "User_" + std::to_string(user_id);
}
```

### Chaining Tasks

```cpp
Task<int> fetch_data();
Task<int> process_data(int data);
Task<void> save_result(int result);

Task<void> workflow() {
    int data = co_await fetch_data();
    int processed = co_await process_data(data);
    co_await save_result(processed);
}
```

### Error Handling

```cpp
Task<int> risky_operation() {
    if (some_condition) {
        throw std::runtime_error("Operation failed");
    }
    co_return 42;
}

Task<void> handle_errors() {
    try {
        int result = co_await risky_operation();
        std::cout << "Success: " << result << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}
```

### Fire and Forget

```cpp
void fire_and_forget() {
    // Don't await - runs concurrently
    auto task = background_work();
    // task destructor will handle cleanup
}
```

### Concurrent Execution

```cpp
Task<std::vector<int>> fetch_all(const std::vector<int>& ids) {
    std::vector<Task<int>> tasks;
    for (int id : ids) {
        tasks.push_back(fetch_data(id));
    }

    std::vector<int> results;
    for (auto& task : tasks) {
        results.push_back(task.result());  // Collect results
    }
    co_return results;
}
```

## Generator Patterns

### Infinite Sequence

```cpp
Generator<int> infinite_counter(int start = 0) {
    while (true) {
        co_yield start++;
    }
}

void use_counter() {
    for (auto n : take(infinite_counter(1), 10)) {
        std::cout << n << " ";
    }
    // Output: 1 2 3 4 5 6 7 8 9 10
}
```

### Fibonacci

```cpp
Generator<long long> fibonacci() {
    long long a = 0, b = 1;
    while (true) {
        co_yield a;
        std::tie(a, b) = std::make_pair(b, a + b);
    }
}
```

### File Line Reader

```cpp
Generator<std::string> read_lines(const std::string& filename) {
    std::ifstream file(filename);
    std::string line;
    while (std::getline(file, line)) {
        co_yield line;
    }
}

void process_file() {
    for (const auto& line : read_lines("data.txt")) {
        process_line(line);
    }
}
```

### Lazy Transform

```cpp
// Process large dataset without loading all into memory
Generator<ProcessedItem> process_items(Generator<RawItem> input) {
    for (auto item : input) {
        co_yield expensive_transform(item);
    }
}
```

## Thread Pool Usage

### Basic Thread Pool

```cpp
void thread_pool_example() {
    ThreadPool pool(4);  // 4 worker threads
    pool.run();

    auto task = []() -> Task<int> {
        std::cout << "Running on thread: " << std::this_thread::get_id() << std::endl;
        co_return 42;
    }();

    pool.schedule(task.get_handle());
    int result = task.result();

    pool.stop();
}
```

### Scheduler Propagation

```cpp
Task<int> child_task() {
    // Automatically runs on parent's scheduler
    co_return compute();
}

Task<int> parent_task(ThreadPool& pool) {
    // This task runs on thread pool
    auto child = child_task();  // Child also runs on thread pool
    co_return co_await child;
}
```

### RAII Executor

```cpp
void raii_example() {
    ThreadPoolExecutor executor(4);  // Starts thread pool

    auto task = my_async_task();
    task.schedule_on(executor.pool());
    int result = task.result();

    // Thread pool automatically stopped when executor goes out of scope
}
```

## Synchronization

### AsyncMutex - Critical Sections

```cpp
AsyncMutex mutex;
int shared_counter = 0;

Task<void> increment_task(int id) {
    co_await mutex.lock();
    // Critical section
    int current = shared_counter;
    shared_counter = current + 1;
    mutex.unlock();
    co_return;
}
```

### Semaphore - Limit Concurrency

```cpp
Semaphore download_sem(5);  // Max 5 concurrent downloads

Task<void> download_file(const std::string& url) {
    co_await download_sem.acquire();

    // Download with limited concurrency
    auto data = co_await http_get(url);
    save_file(data);

    download_sem.release();
}
```

### BinarySemaphore - Signaling

```cpp
BinarySemaphore data_ready;
std::string shared_data;

Task<void> producer() {
    shared_data = "Hello from producer!";
    data_ready.release();
    co_return;
}

Task<void> consumer() {
    co_await data_ready.acquire();
    std::cout << "Received: " << shared_data << std::endl;
    co_return;
}
```

### Channel - Producer/Consumer

```cpp
Channel<int> data_channel(100);  // Buffer 100 items

Task<void> producer() {
    for (int i = 0; i < 1000; ++i) {
        bool sent = co_await data_channel.send(i);
        if (!sent) {
            std::cout << "Channel closed" << std::endl;
            co_return;
        }
    }
    data_channel.close();
}

Task<void> consumer() {
    while (true) {
        auto value = co_await data_channel.receive();
        if (!value) {
            std::cout << "Channel closed, exiting" << std::endl;
            co_return;
        }
        process(*value);
    }
}
```

## Memory Management

### Memory Pool

```cpp
// Fixed-size pool
MemoryPool<64> pool;

void* ptr = pool.allocate();
// Use memory...
pool.deallocate(ptr);
```

### Size-Class Pool

```cpp
SizeClassPool& pool = SizeClassPool::instance();

// Automatically selects appropriate pool
void* small = pool.allocate(32);
void* medium = pool.allocate(256);
void* large = pool.allocate(4096);

// Deallocate with size
pool.deallocate(small, 32);
pool.deallocate(medium, 256);
pool.deallocate(large, 4096);
```

### Pool Allocator with STL

```cpp
// Vector with pool allocator
std::vector<int, PoolAllocator<int>> vec;
vec.reserve(1000);

// String with pool allocator
std::basic_string<char, std::char_traits<char>, PoolAllocator<char>> str;
str = "Pool-allocated string";
```

### Object Pool

```cpp
struct HttpRequest {
    std::string method;
    std::string path;
    std::vector<std::string> headers;

    void reset() {
        method.clear();
        path.clear();
        headers.clear();
    }
};

ObjectPool<HttpRequest, 64> request_pool;

void handle_connection() {
    auto req = request_pool.acquire();
    req->method = "GET";
    req->path = "/api/users";
    // Process request...
    // Automatically returned to pool when req goes out of scope
}
```

### Coroutine Frame Pool

```cpp
// In your promise type
struct MyTaskPromise {
    CORO_FRAME_ALLOCATOR  // Enables pooled allocation

    MyTask get_return_object() { /* ... */ }
    // ...
};
```

## HTTP Server

### Basic Server

```cpp
#include "coro/coro.hpp"
using namespace coro::net::http;

int main() {
    ThreadPool pool(4);
    Server server(pool, 8080);

    server.get("/", [](const Request& req, Response& resp) -> Task<void> {
        resp.html("<h1>Hello, World!</h1>");
        co_return;
    });

    server.bind("0.0.0.0");
    server.start();
    return 0;
}
```

### REST API

```cpp
server
    .get("/api/users", [](const Request& req, Response& resp) -> Task<void> {
        resp.json(R"([
            {"id": 1, "name": "Alice"},
            {"id": 2, "name": "Bob"}
        ])");
        co_return;
    })
    .post("/api/users", [](const Request& req, Response& resp) -> Task<void> {
        // Parse request body
        resp.status(Status::Created).json(R"({"id": 3})");
        co_return;
    })
    .get("/api/users/:id", [](const Request& req, Response& resp) -> Task<void> {
        std::string id = extract_id(req.path());
        resp.json("{\"id\":" + id + ",\"name\":\"User " + id + "\"}");
        co_return;
    });
```

### Handling Query Parameters

```cpp
server.get("/api/search", [](const Request& req, Response& resp) -> Task<void> {
    std::string query = req.query_param("q");
    int limit = std::stoi(req.query_param("limit"));

    auto results = search(query, limit);
    resp.json(format_results(results));
    co_return;
});
```

### Response Helpers

```cpp
// Different response types
resp.text("Plain text response");
resp.html("<html>...</html>");
resp.json(R"({"key": "value"})");
resp.redirect("/new-location");

// Custom status
resp.status(Status::NotFound).text("Resource not found");

// Headers
resp.header("X-Custom-Header", "value");
resp.content_type("application/xml");
```

## Best Practices

### 1. Use RAII for Resources

```cpp
// Good: RAII
Task<void> good_example() {
    auto file = open_file("data.txt");
    auto data = co_await file.read();
    // File automatically closed
}

// Bad: Manual resource management
Task<void> bad_example() {
    FILE* f = fopen("data.txt", "r");
    auto data = co_await read_file(f);
    fclose(f);  // Might not execute if exception thrown
}
```

### 2. Avoid Blocking Operations

```cpp
// Good: Async operation
Task<void> good_async() {
    auto data = co_await async_read_file("data.txt");
}

// Bad: Blocking the thread
Task<void> bad_blocking() {
    auto data = sync_read_file("data.txt");  // Blocks thread!
}
```

### 3. Handle Exceptions

```cpp
Task<void> robust_operation() {
    try {
        co_await risky_task();
    } catch (const NetworkError& e) {
        // Handle network error
    } catch (const std::exception& e) {
        // Handle other errors
    }
}
```

### 4. Use Structured Concurrency

```cpp
// Good: Structured - all tasks complete before function returns
Task<void> structured_example() {
    auto t1 = task1();
    auto t2 = task2();
    auto t3 = task3();

    co_await t1;
    co_await t2;
    co_await t3;
}
```

### 5. Prefer Thread Pool for CPU Work

```cpp
Task<int> cpu_intensive() {
    // Run on thread pool to avoid blocking event loop
    co_return co_await run_on_thread_pool([]() {
        return heavy_computation();
    });
}
```

## Common Pitfalls

### 1. Dangling References

```cpp
// Bad: Reference to local variable
task = [&]() -> Task<int> {
    int local = 42;
    co_return local;  // Returning reference to local!
}();

// Good: Capture by value
task = [=]() -> Task<int> {
    int local = 42;
    co_return local;
}();
```

### 2. Not Awaiting Tasks

```cpp
// Bad: Task may not complete
void fire_and_forget_bad() {
    my_task();  // Temporary destroyed immediately!
}

// Good: Store the task
void fire_and_forget_good() {
    auto task = my_task();
    // task will complete before destruction
}
```

### 3. Thread Safety

```cpp
// Bad: Unsynchronized access
int counter = 0;

Task<void> unsafe_increment() {
    ++counter;  // Data race!
    co_return;
}

// Good: Use synchronization
AsyncMutex mutex;
int counter = 0;

Task<void> safe_increment() {
    co_await mutex.lock();
    ++counter;
    mutex.unlock();
    co_return;
}
```

### 4. Deadlocks

```cpp
// Bad: Holding lock while awaiting
Task<void> deadlock_risk() {
    co_await mutex.lock();
    auto result = co_await async_operation();  // May never resume!
    mutex.unlock();
}

// Good: Release lock before awaiting
Task<void> no_deadlock() {
    {
        co_await mutex.lock();
        auto data = shared_data;
        mutex.unlock();
    }
    auto result = co_await async_operation();
}
```

### 5. Stack Overflow

```cpp
// Bad: Recursive coroutines without suspension
Task<void> bad_recursive(int n) {
    if (n > 0) {
        co_await bad_recursive(n - 1);  // Stack overflow risk
    }
}

// Good: Add suspension points
Task<void> good_recursive(int n) {
    if (n > 0) {
        co_await std::suspend_always{};  // Trampoline
        co_await good_recursive(n - 1);
    }
}
```

## Debugging Tips

### Enable Coroutine Tracing

```cpp
#ifdef CORO_DEBUG
    std::cout << "Coroutine " << this << " started" << std::endl;
#endif
```

### Check Scheduler Association

```cpp
Task<void> check_scheduler() {
    if (Scheduler::current()) {
        std::cout << "Running on scheduler" << std::endl;
    } else {
        std::cout << "Running inline" << std::endl;
    }
    co_return;
}
```

### Detect Use-After-Free

```cpp
// Use shared_ptr for shared state
Task<void> safe_shared_state() {
    auto state = std::make_shared<State>();
    co_await async_op([state]() { /* use state */ });
    // state kept alive by lambda capture
}
```
