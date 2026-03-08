#pragma once

#include <coroutine>
#include <exception>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace coro {

// Forward declaration
class Scheduler;

namespace detail {

// Base promise type with common functionality
template <typename T>
class TaskPromiseBase {
public:
    struct FinalAwaiter {
        bool await_ready() const noexcept { return false; }

        template <typename Promise>
        std::coroutine_handle<> await_suspend(
            std::coroutine_handle<Promise> h) noexcept {
            // Resume the continuation if it exists
            if (auto& cont = h.promise().continuation_; cont) {
                return cont;
            }
            return std::noop_coroutine();
        }

        void await_resume() noexcept {}
    };

    TaskPromiseBase() = default;
    ~TaskPromiseBase() = default;

    // Disable copy and move
    TaskPromiseBase(const TaskPromiseBase&) = delete;
    TaskPromiseBase& operator=(const TaskPromiseBase&) = delete;
    TaskPromiseBase(TaskPromiseBase&&) = delete;
    TaskPromiseBase& operator=(TaskPromiseBase&&) = delete;

    std::suspend_always initial_suspend() noexcept { return {}; }

    FinalAwaiter final_suspend() noexcept { return {}; }

    void unhandled_exception() noexcept {
        exception_ = std::current_exception();
    }

    void set_continuation(std::coroutine_handle<> cont) noexcept {
        continuation_ = cont;
    }

    std::coroutine_handle<> continuation() const noexcept {
        return continuation_;
    }

    std::exception_ptr exception() const noexcept { return exception_; }

    void rethrow_if_exception() const {
        if (exception_) {
            std::rethrow_exception(exception_);
        }
    }

    Scheduler* scheduler() const noexcept { return scheduler_; }
    void set_scheduler(Scheduler* sched) noexcept { scheduler_ = sched; }

private:
    std::coroutine_handle<> continuation_;
    std::exception_ptr exception_;
    Scheduler* scheduler_ = nullptr;
};

}  // namespace detail

// Forward declaration for Task
template <typename T>
class Task;

// Specialization for void
template <>
class Task<void>;

// Task awaiter used when awaiting another Task
template <typename T>
class TaskAwaiter {
public:
    explicit TaskAwaiter(Task<T>&& task) : task_(std::move(task)) {}

    TaskAwaiter(const TaskAwaiter&) = delete;
    TaskAwaiter& operator=(const TaskAwaiter&) = delete;

    TaskAwaiter(TaskAwaiter&& other) noexcept
        : task_(std::exchange(other.task_, {})) {}

    TaskAwaiter& operator=(TaskAwaiter&& other) noexcept {
        if (this != &other) {
            task_ = std::exchange(other.task_, {});
        }
        return *this;
    }

    bool await_ready() const noexcept {
        return !task_.handle_ || task_.handle_.done();
    }

    template <typename Promise>
    std::coroutine_handle<> await_suspend(
        std::coroutine_handle<Promise> h) noexcept {
        // Set this coroutine as the continuation of the awaited task
        task_.handle_.promise().set_continuation(h);
        return task_.handle_;
    }

    T await_resume();

private:
    Task<T> task_;
};

// Task<T> - A coroutine that produces a value of type T
template <typename T>
class Task {
public:
    struct Promise : detail::TaskPromiseBase<T> {
        Task get_return_object() {
            return Task{std::coroutine_handle<Promise>::from_promise(*this)};
        }

        template <typename U>
            requires std::convertible_to<U, T>
        void return_value(U&& value) noexcept(
            std::is_nothrow_constructible_v<T, U>) {
            value_.template emplace<T>(std::forward<U>(value));
        }

        T& result() & {
            this->rethrow_if_exception();
            return value_.value();
        }

        T&& result() && {
            this->rethrow_if_exception();
            return std::move(value_.value());
        }

        const T& result() const& {
            this->rethrow_if_exception();
            return value_.value();
        }

    private:
        std::optional<T> value_;
    };

    using promise_type = Promise;
    using value_type = T;

    // Awaiter type for co_await
    class Awaiter {
    public:
        explicit Awaiter(std::coroutine_handle<Promise> h) : handle_(h) {}

        bool await_ready() const noexcept {
            return !handle_ || handle_.done();
        }

        std::coroutine_handle<> await_suspend(
            std::coroutine_handle<> h) noexcept {
            handle_.promise().set_continuation(h);
            return handle_;
        }

        T await_resume() {
            if (!handle_) {
                throw std::runtime_error("Awaiting invalid Task");
            }
            return std::move(handle_.promise()).result();
        }

    private:
        std::coroutine_handle<Promise> handle_;
    };

    Task() = default;

    explicit Task(std::coroutine_handle<Promise> h) : handle_(h) {}

    ~Task() {
        if (handle_) {
            handle_.destroy();
        }
    }

    // Disable copy
    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;

    // Enable move
    Task(Task&& other) noexcept : handle_(std::exchange(other.handle_, {})) {}

    Task& operator=(Task&& other) noexcept {
        if (this != &other) {
            if (handle_) {
                handle_.destroy();
            }
            handle_ = std::exchange(other.handle_, {});
        }
        return *this;
    }

    // Check if the task has completed
    [[nodiscard]] bool is_ready() const noexcept {
        return !handle_ || handle_.done();
    }

    // Resume the coroutine (should be called by scheduler)
    void resume() {
        if (handle_ && !handle_.done()) {
            handle_.resume();
        }
    }

    // Get the result (only valid after task completes)
    T result() {
        if (!handle_) {
            throw std::runtime_error("Task has no coroutine handle");
        }
        // Resume the coroutine if it hasn't completed yet
        if (!handle_.done()) {
            handle_.resume();
        }
        return std::move(handle_.promise()).result();
    }

    // Support for co_await
    Awaiter operator co_await() && {
        if (!handle_) {
            throw std::runtime_error("Cannot await empty Task");
        }
        return Awaiter{handle_};
    }

    // Get the underlying handle (for scheduler use)
    std::coroutine_handle<Promise> handle() const noexcept { return handle_; }

    // Check if valid
    [[nodiscard]] explicit operator bool() const noexcept {
        return handle_ != nullptr;
    }

    void detach() { handle_ = nullptr; }

private:
    std::coroutine_handle<Promise> handle_;
};

// Task<void> specialization
template <>
class Task<void> {
public:
    struct Promise : detail::TaskPromiseBase<void> {
        Task get_return_object() {
            return Task{std::coroutine_handle<Promise>::from_promise(*this)};
        }

        void return_void() noexcept {}

        void result() { this->rethrow_if_exception(); }
    };

    using promise_type = Promise;
    using value_type = void;

    class Awaiter {
    public:
        explicit Awaiter(std::coroutine_handle<Promise> h) : handle_(h) {}

        bool await_ready() const noexcept {
            return !handle_ || handle_.done();
        }

        std::coroutine_handle<> await_suspend(
            std::coroutine_handle<> h) noexcept {
            handle_.promise().set_continuation(h);
            return handle_;
        }

        void await_resume() {
            if (!handle_) {
                throw std::runtime_error("Awaiting invalid Task");
            }
            handle_.promise().result();
        }

    private:
        std::coroutine_handle<Promise> handle_;
    };

    Task() = default;

    explicit Task(std::coroutine_handle<Promise> h) : handle_(h) {}

    ~Task() {
        if (handle_) {
            handle_.destroy();
        }
    }

    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;

    Task(Task&& other) noexcept : handle_(std::exchange(other.handle_, {})) {}

    Task& operator=(Task&& other) noexcept {
        if (this != &other) {
            if (handle_) {
                handle_.destroy();
            }
            handle_ = std::exchange(other.handle_, {});
        }
        return *this;
    }

    [[nodiscard]] bool is_ready() const noexcept {
        return !handle_ || handle_.done();
    }

    void resume() {
        if (handle_ && !handle_.done()) {
            handle_.resume();
        }
    }

    void result() {
        if (!handle_) {
            throw std::runtime_error("Task has no coroutine handle");
        }
        // Resume the coroutine if it hasn't completed yet
        if (!handle_.done()) {
            handle_.resume();
        }
        handle_.promise().result();
    }

    Awaiter operator co_await() && {
        if (!handle_) {
            throw std::runtime_error("Cannot await empty Task");
        }
        return Awaiter{handle_};
    }

    std::coroutine_handle<Promise> handle() const noexcept { return handle_; }

    [[nodiscard]] explicit operator bool() const noexcept {
        return handle_ != nullptr;
    }

    void detach() { handle_ = nullptr; }

private:
    std::coroutine_handle<Promise> handle_;
};

// TaskAwaiter<T>::await_resume() implementation
template <typename T>
T TaskAwaiter<T>::await_resume() {
    if (!task_) {
        throw std::runtime_error("Awaiting invalid Task");
    }
    return std::move(task_.handle_.promise()).result();
}

// Helper functions

// Create a ready Task with a value
template <typename T>
Task<T> make_task(T&& value) {
    co_return std::forward<T>(value);
}

inline Task<void> make_task() { co_return; }

}  // namespace coro
