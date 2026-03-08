#pragma once

#include "coro/core/task.hpp"

#include <atomic>
#include <coroutine>
#include <cstddef>
#include <mutex>
#include <optional>
#include <queue>

namespace coro {

// BinarySemaphore - A semaphore with count 0 or 1
class BinarySemaphore {
public:
    explicit BinarySemaphore(bool signaled = false)
        : signaled_(signaled ? 1 : 0) {}

    ~BinarySemaphore() = default;

    // Disable copy and move
    BinarySemaphore(const BinarySemaphore&) = delete;
    BinarySemaphore& operator=(const BinarySemaphore&) = delete;
    BinarySemaphore(BinarySemaphore&&) = delete;
    BinarySemaphore& operator=(BinarySemaphore&&) = delete;

    // Awaitable acquire operation
    class AcquireAwaiter {
    public:
        explicit AcquireAwaiter(BinarySemaphore& sem) : sem_(sem) {}

        [[nodiscard]] bool await_ready() const noexcept {
            return sem_.try_acquire();
        }

        void await_suspend(std::coroutine_handle<> handle) {
            sem_.enqueue_waiter(handle);
        }

        void await_resume() noexcept {}

    private:
        BinarySemaphore& sem_;
    };

    // Acquire the semaphore (suspend if not available)
    [[nodiscard]] AcquireAwaiter acquire() {
        return AcquireAwaiter(*this);
    }

    // Try to acquire without blocking
    [[nodiscard]] bool try_acquire() noexcept {
        int expected = 1;
        return signaled_.compare_exchange_strong(expected, 0,
            std::memory_order_acquire, std::memory_order_relaxed);
    }

    // Release the semaphore
    void release() {
        std::optional<std::coroutine_handle<>> waiter;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!waiters_.empty()) {
                waiter = waiters_.front();
                waiters_.pop();
            } else {
                signaled_.store(1, std::memory_order_release);
            }
        }

        if (waiter) {
            waiter->resume();
        }
    }

private:
    void enqueue_waiter(std::coroutine_handle<> handle) {
        std::lock_guard<std::mutex> lock(mutex_);
        waiters_.push(handle);
    }

    std::atomic<int> signaled_;
    std::mutex mutex_;
    std::queue<std::coroutine_handle<>> waiters_;
};

// CountingSemaphore - A semaphore with a maximum count
class CountingSemaphore {
public:
    explicit CountingSemaphore(std::ptrdiff_t max_count)
        : max_count_(max_count), count_(max_count) {}

    ~CountingSemaphore() = default;

    // Disable copy and move
    CountingSemaphore(const CountingSemaphore&) = delete;
    CountingSemaphore& operator=(const CountingSemaphore&) = delete;
    CountingSemaphore(CountingSemaphore&&) = delete;
    CountingSemaphore& operator=(CountingSemaphore&&) = delete;

    // Awaitable acquire operation
    class AcquireAwaiter {
    public:
        explicit AcquireAwaiter(CountingSemaphore& sem, std::ptrdiff_t n = 1)
            : sem_(sem), n_(n) {}

        [[nodiscard]] bool await_ready() const noexcept {
            return sem_.try_acquire(n_);
        }

        void await_suspend(std::coroutine_handle<> handle) {
            sem_.enqueue_waiter(handle, n_);
        }

        void await_resume() noexcept {}

    private:
        CountingSemaphore& sem_;
        std::ptrdiff_t n_;
    };

    // Acquire n permits (suspend if not available)
    [[nodiscard]] AcquireAwaiter acquire(std::ptrdiff_t n = 1) {
        return AcquireAwaiter(*this, n);
    }

    // Try to acquire n permits without blocking
    [[nodiscard]] bool try_acquire(std::ptrdiff_t n = 1) noexcept {
        std::ptrdiff_t expected = count_.load(std::memory_order_relaxed);
        do {
            if (expected < n) {
                return false;
            }
        } while (!count_.compare_exchange_weak(expected, expected - n,
            std::memory_order_acquire, std::memory_order_relaxed));
        return true;
    }

    // Release n permits
    void release(std::ptrdiff_t n = 1) {
        std::vector<std::coroutine_handle<>> to_resume;
        {
            std::lock_guard<std::mutex> lock(mutex_);

            // Add permits
            std::ptrdiff_t new_count = count_.fetch_add(n, std::memory_order_release) + n;
            if (new_count > max_count_) {
                count_.store(max_count_, std::memory_order_relaxed);
            }

            // Check if we can resume any waiters
            while (!waiters_.empty()) {
                auto& [handle, needed] = waiters_.front();
                if (count_.load(std::memory_order_relaxed) >= needed) {
                    count_.fetch_sub(needed, std::memory_order_acquire);
                    to_resume.push_back(handle);
                    waiters_.pop();
                } else {
                    break;
                }
            }
        }

        // Resume waiters outside the lock
        for (auto& handle : to_resume) {
            handle.resume();
        }
    }

private:
    void enqueue_waiter(std::coroutine_handle<> handle, std::ptrdiff_t n) {
        std::lock_guard<std::mutex> lock(mutex_);
        waiters_.push({handle, n});
    }

    std::ptrdiff_t max_count_;
    std::atomic<std::ptrdiff_t> count_;
    std::mutex mutex_;
    std::queue<std::pair<std::coroutine_handle<>, std::ptrdiff_t>> waiters_;
};

// Type alias for convenience
using Semaphore = CountingSemaphore;

} // namespace coro
