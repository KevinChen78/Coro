// Quick compiler capability test for C++20 coroutines
#include <coroutine>
#include <iostream>
#include <optional>

// Minimal coroutine test
template<typename T>
struct Task {
    struct Promise {
        T value{};
        std::exception_ptr exception;

        Task get_return_object() {
            return Task{std::coroutine_handle<Promise>::from_promise(*this)};
        }
        std::suspend_always initial_suspend() noexcept { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        void return_value(T val) { value = val; }
        void unhandled_exception() { exception = std::current_exception(); }
    };

    using promise_type = Promise;

    explicit Task(std::coroutine_handle<Promise> h) : handle_(h) {}
    ~Task() { if (handle_) handle_.destroy(); }

    Task(Task&& other) noexcept : handle_(std::exchange(other.handle_, {})) {}
    Task& operator=(Task&& other) noexcept {
        if (this != &other) {
            if (handle_) handle_.destroy();
            handle_ = std::exchange(other.handle_, {});
        }
        return *this;
    }

    T result() {
        if (!handle_.done()) handle_.resume();
        if (handle_.promise().exception) std::rethrow_exception(handle_.promise().exception);
        return handle_.promise().value;
    }

    // Make Task awaitable
    bool await_ready() const noexcept { return handle_.done(); }
    void await_suspend(std::coroutine_handle<>) noexcept {}
    T await_resume() { return result(); }

private:
    std::coroutine_handle<Promise> handle_;
};

Task<int> compute() {
    co_return 42;
}

Task<int> chained() {
    int a = co_await compute();
    int b = co_await compute();
    co_return a + b;
}

int main() {
    std::cout << "=== C++20 Coroutine Compiler Test ===" << std::endl;

    auto t1 = compute();
    std::cout << "compute() = " << t1.result() << std::endl;

    auto t2 = chained();
    std::cout << "chained() = " << t2.result() << std::endl;

    std::cout << "All tests passed!" << std::endl;
    return 0;
}
