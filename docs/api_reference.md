# Coro API Reference

## Core Components

### Task<T>

A coroutine that produces a value of type T.

```cpp
template<typename T>
class Task {
public:
    using promise_type = TaskPromise<T>;

    // Construction
    Task(std::coroutine_handle<promise_type> handle);
    Task(Task&& other) noexcept;
    Task& operator=(Task&& other) noexcept;

    // Destruction
    ~Task();

    // Result access
    [[nodiscard]] T result();
    [[nodiscard]] bool is_ready() const noexcept;

    // Scheduling
    void schedule(Scheduler& scheduler);
    std::coroutine_handle<promise_type> get_handle();
};
```

**Member Functions**:

- `result()`: Blocks until task completes and returns the value. Rethrows exceptions.
- `is_ready()`: Checks if task has completed without blocking.
- `schedule()`: Associates task with a scheduler for execution.
- `get_handle()`: Returns the coroutine handle.

### Generator<T>

A coroutine that produces a sequence of values.

```cpp
template<typename T>
class Generator {
public:
    using promise_type = GeneratorPromise<T>;
    using iterator = GeneratorIterator<T>;

    // Construction
    Generator(std::coroutine_handle<promise_type> handle);
    Generator(Generator&& other) noexcept;
    Generator& operator=(Generator&& other) noexcept;

    // Destruction
    ~Generator();

    // Iteration
    [[nodiscard]] iterator begin();
    [[nodiscard]] iterator end() const noexcept;
};
```

**Member Functions**:

- `begin()`: Returns iterator to first element (resumes coroutine).
- `end()`: Returns sentinel iterator.

## Scheduler

### Scheduler Interface

```cpp
class Scheduler {
public:
    virtual ~Scheduler() = default;
    virtual void schedule(std::coroutine_handle<> handle) = 0;
    virtual void run() = 0;
    virtual void stop() = 0;
    virtual bool is_on_scheduler_thread() const = 0;
};
```

### InlineScheduler

Executes coroutines immediately in the calling thread.

```cpp
class InlineScheduler : public Scheduler {
public:
    void schedule(std::coroutine_handle<> handle) override;
    void run() override;
    void stop() override;
    bool is_on_scheduler_thread() const override;
};
```

### ThreadPool

Work-stealing thread pool scheduler.

```cpp
class ThreadPool : public Scheduler {
public:
    explicit ThreadPool(size_t num_threads);
    ~ThreadPool();

    void schedule(std::coroutine_handle<> handle) override;
    void run() override;
    void stop() override;
    bool is_on_scheduler_thread() const override;

    size_t num_threads() const noexcept;
    size_t thread_id() const noexcept;
    void schedule_on(size_t thread_index, std::coroutine_handle<> handle);
    static void yield_current();
};
```

**Member Functions**:

- `run()`: Starts worker threads.
- `stop()`: Stops worker threads.
- `schedule_on()`: Schedules to specific thread.
- `yield_current()`: Yields current thread to other tasks.

## Synchronization

### AsyncMutex

Coroutine-aware mutex.

```cpp
class AsyncMutex {
public:
    AsyncMutex() = default;
    ~AsyncMutex() = default;

    LockAwaiter lock();
    bool try_lock();
    void unlock();
};
```

**Member Functions**:

- `lock()`: Awaitable lock acquisition.
- `try_lock()`: Non-blocking lock attempt.
- `unlock()`: Release lock, resumes waiters.

### Semaphore

Counting semaphore.

```cpp
class CountingSemaphore {
public:
    explicit CountingSemaphore(std::ptrdiff_t max_count);
    ~CountingSemaphore() = default;

    AcquireAwaiter acquire(std::ptrdiff_t n = 1);
    bool try_acquire(std::ptrdiff_t n = 1);
    void release(std::ptrdiff_t n = 1);
};

using Semaphore = CountingSemaphore;
```

**Member Functions**:

- `acquire(n)`: Awaitable acquisition of n permits.
- `try_acquire(n)`: Non-blocking acquisition.
- `release(n)`: Release n permits.

### BinarySemaphore

Binary (0/1) semaphore.

```cpp
class BinarySemaphore {
public:
    explicit BinarySemaphore(bool signaled = false);

    AcquireAwaiter acquire();
    bool try_acquire();
    void release();
};
```

### Channel<T>

Buffered channel for coroutine communication.

```cpp
template<typename T>
class Channel {
public:
    explicit Channel(size_t capacity);
    ~Channel() = default;

    Task<bool> send(T value);
    Task<std::optional<T>> receive();

    bool try_send(const T& value);
    std::optional<T> try_receive();

    void close();
    bool is_closed() const noexcept;
    size_t size() const;
    bool empty() const;
};
```

**Member Functions**:

- `send()`: Awaitable send operation.
- `receive()`: Awaitable receive operation.
- `try_send()`: Non-blocking send.
- `try_receive()`: Non-blocking receive.
- `close()`: Close channel.

## Network I/O

### TcpStream

TCP socket stream.

```cpp
class TcpStream {
public:
    TcpStream();
    explicit TcpStream(Socket socket);
    ~TcpStream() = default;

    // Read operations
    Task<std::optional<size_t>> read(std::vector<uint8_t>& buffer);
    Task<bool> read_exact(std::vector<uint8_t>& buffer, size_t len);
    Task<std::optional<std::string>> read_until(char delimiter);
    Task<std::optional<std::string>> read_line();

    // Write operations
    Task<std::optional<size_t>> write(const uint8_t* data, size_t len);
    Task<std::optional<size_t>> write(const std::string& str);
    Task<bool> write_all(const uint8_t* data, size_t len);
    Task<bool> write_all(const std::string& str);

    bool is_connected() const noexcept;
    void close();
};
```

### TcpListener

TCP socket listener.

```cpp
class TcpListener {
public:
    TcpListener();
    ~TcpListener();

    bool bind(const std::string& host, uint16_t port);
    std::optional<TcpStream> accept();
    void stop();
    bool is_bound() const noexcept;
};
```

## HTTP

### Request

HTTP request parser.

```cpp
class Request {
public:
    bool parse(const std::string& data);

    Method method() const noexcept;
    const std::string& path() const noexcept;
    const std::string& version() const noexcept;
    const std::string& body() const noexcept;

    std::string header(const std::string& name) const;
    bool has_header(const std::string& name) const;
    std::string query_param(const std::string& name) const;

    size_t content_length() const;
    bool is_keep_alive() const;
};
```

### Response

HTTP response builder.

```cpp
class Response {
public:
    Response& status(Status code);
    Response& version(const std::string& version);
    Response& header(const std::string& name, const std::string& value);
    Response& content_type(const std::string& type);
    Response& body(const std::string& content);

    Response& json(const std::string& json_str);
    Response& html(const std::string& html_content);
    Response& text(const std::string& text_content);
    Response& redirect(const std::string& location, Status code = Status::Found);
    Response& keep_alive(bool enabled);

    std::string build() const;
    std::vector<uint8_t> to_bytes() const;

    // Factory methods
    static Response ok(const std::string& body = "");
    static Response not_found(const std::string& message = "Not Found");
    static Response bad_request(const std::string& message = "Bad Request");
    static Response server_error(const std::string& message = "Internal Server Error");
    static Response json_response(const std::string& json);
};
```

### Server

HTTP server.

```cpp
class Server {
public:
    Server(ThreadPool& pool, uint16_t port = 8080);
    ~Server();

    bool bind(const std::string& host = "0.0.0.0");
    void start();
    void stop();
    bool is_running() const noexcept;

    Router& router() noexcept;

    // Routing helpers
    Server& route(Method method, const std::string& path, Handler handler);
    Server& get(const std::string& path, Handler handler);
    Server& post(const std::string& path, Handler handler);
    Server& put(const std::string& path, Handler handler);
    Server& del(const std::string& path, Handler handler);
};
```

## Memory

### MemoryPool

Fixed-size memory pool.

```cpp
template<size_t BlockSize, size_t BlocksPerChunk = 64>
class MemoryPool {
public:
    MemoryPool();
    ~MemoryPool();

    void* allocate();
    void deallocate(void* ptr);
    size_t block_size() const noexcept;
};
```

### SizeClassPool

Size-class based memory pool.

```cpp
class SizeClassPool {
public:
    static SizeClassPool& instance();

    void* allocate(size_t size);
    void deallocate(void* ptr, size_t size);
};
```

### PoolAllocator

STL-compatible allocator.

```cpp
template<typename T>
class PoolAllocator {
public:
    using value_type = T;

    PoolAllocator() noexcept = default;
    template<typename U> PoolAllocator(const PoolAllocator<U>&) noexcept {}

    T* allocate(size_t n);
    void deallocate(T* ptr, size_t n);
};
```

### ObjectPool

Object reuse pool.

```cpp
template<typename T, size_t InitialCapacity = 64>
class ObjectPool {
public:
    ObjectPool();
    ~ObjectPool();

    std::unique_ptr<T, std::function<void(T*)>> acquire();
    size_t available_count() const;
    void clear();
};
```

## Enums

### Method

HTTP methods.

```cpp
enum class Method {
    GET,
    POST,
    PUT,
    DEL,  // DELETE is Windows macro
    HEAD,
    OPTIONS,
    PATCH,
    CONNECT,
    TRACE,
    UNKNOWN
};
```

### Status

HTTP status codes.

```cpp
enum class Status : uint16_t {
    OK = 200,
    Created = 201,
    Accepted = 202,
    NoContent = 204,
    MovedPermanently = 301,
    Found = 302,
    NotModified = 304,
    BadRequest = 400,
    Unauthorized = 401,
    Forbidden = 403,
    NotFound = 404,
    MethodNotAllowed = 405,
    Conflict = 409,
    InternalServerError = 500,
    NotImplemented = 501,
    BadGateway = 502,
    ServiceUnavailable = 503
};
```

## Macros

### CORO_FRAME_ALLOCATOR

Enables pooled allocation for coroutine frames.

```cpp
struct Promise {
    CORO_FRAME_ALLOCATOR
    // ...
};
```

## Type Aliases

```cpp
using Semaphore = CountingSemaphore;
```

## Helper Functions

### Method Conversion

```cpp
Method method_from_string(const std::string& str);
std::string method_to_string(Method method);
```

### Status Text

```cpp
std::string status_text(Status status);
```
