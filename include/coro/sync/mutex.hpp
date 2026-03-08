#pragma once

#include "coro/core/task.hpp"

#include <atomic>
#include <coroutine>
#include <mutex>
#include <optional>
#include <queue>

namespace coro {

// AsyncMutex - A mutex that can be awaited from coroutines
// Unlike std::mutex, this does not block the thread, only suspends the coroutine
class AsyncMutex {
public:
    AsyncMutex() = default;
    ~AsyncMutex() = default;

    // Disable copy and move
    AsyncMutex(const AsyncMutex&) = delete;
    AsyncMutex& operator=(const AsyncMutex&) = delete;
    AsyncMutex(AsyncMutex&&) = delete;
    AsyncMutex& operator=(AsyncMutex&&) = delete;

    // Lock guard for RAII-style locking
    class LockGuard {
    public:
        explicit LockGuard(AsyncMutex& mutex) : mutex_(mutex), locked_(false) {}

        LockGuard(const LockGuard&) = delete;
        LockGuard& operator=(const LockGuard&) = delete;

        LockGuard(LockGuard&& other) noexcept
            : mutex_(other.mutex_), locked_(other.locked_) {
            other.locked_ = false;
        }

        LockGuard& operator=(LockGuard&& other) = delete;

        ~LockGuard() {
            if (locked_) {
                mutex_.unlock();
            }
        }

        [[nodiscard]] Task<void> lock() {
            co_await mutex_.lock();
            locked_ = true;
        }

        void unlock() {
            if (locked_) {
                mutex_.unlock();
                locked_ = false;
            }
        }

        [[nodiscard]] bool owns_lock() const noexcept { return locked_; }

    private:
        AsyncMutex& mutex_;
        bool locked_;
    };

    // Awaitable lock operation
    class LockAwaiter {
    public:
        explicit LockAwaiter(AsyncMutex& mutex) : mutex_(mutex) {}

        [[nodiscard]] bool await_ready() const noexcept {
            return mutex_.try_lock();
        }

        void await_suspend(std::coroutine_handle<> handle) {
            mutex_.enqueue_waiter(handle);
        }

        void await_resume() noexcept {}

    private:
        AsyncMutex& mutex_;
    };

    // Acquire the lock (suspend if not available)
    [[nodiscard]] LockAwaiter lock() {
        return LockAwaiter(*this);
    }

    // Try to acquire the lock without blocking
    [[nodiscard]] bool try_lock() noexcept {
        bool expected = false;
        return locked_.compare_exchange_strong(expected, true,
            std::memory_order_acquire, std::memory_order_relaxed);
    }

    // Release the lock
    void unlock() {
        // Release lock
        locked_.store(false, std::memory_order_release);

        // Resume a waiting coroutine if any
        std::optional<std::coroutine_handle<>> waiter;
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            if (!waiters_.empty()) {
                waiter = waiters_.front();
                waiters_.pop();
            }
        }

        if (waiter) {
            // Try to acquire lock for the waiter
            bool expected = false;
            if (locked_.compare_exchange_strong(expected, true,
                    std::memory_order_acquire, std::memory_order_relaxed)) {
                waiter->resume();
            } else {
                // Lock was reacquired by another thread, re-queue the waiter
                std::lock_guard<std::mutex> lock(queue_mutex_);
                waiters_.push(*waiter);
            }
        }
    }

    // Create a lock guard
    [[nodiscard]] LockGuard guard() {
        return LockGuard(*this);
    }

private:
    void enqueue_waiter(std::coroutine_handle<> handle) {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        waiters_.push(handle);
    }

    std::atomic<bool> locked_{false};
    std::mutex queue_mutex_;
    std::queue<std::coroutine_handle<>> waiters_;
};

// Scoped lock guard for AsyncMutex
template <typename Mutex>
class [[nodiscard]] ScopedLock {
public:
    explicit ScopedLock(Mutex& mutex) : mutex_(mutex), locked_(false) {}

    ScopedLock(const ScopedLock&) = delete;
    ScopedLock& operator=(const ScopedLock&) = delete;
    ScopedLock(ScopedLock&&) = delete;
    ScopedLock& operator=(ScopedLock&&) = delete;

    ~ScopedLock() {
        if (locked_) {
            mutex_.unlock();
        }
    }

    [[nodiscard]] Task<void> acquire() {
        co_await mutex_.lock();
        locked_ = true;
    }

    void release() {
        if (locked_) {
            mutex_.unlock();
            locked_ = false;
        }
    }

private:
    Mutex& mutex_;
    bool locked_;
};

// Syntactic sugar for lock guard
using AsyncLockGuard = ScopedLock<AsyncMutex>;

} // namespace coro
