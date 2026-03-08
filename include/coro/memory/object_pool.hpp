#pragma once

#include <atomic>
#include <cstddef>
#include <memory>
#include <mutex>
#include <queue>
#include <type_traits>
#include <utility>

namespace coro {
namespace memory {

// Object pool for reusing objects of type T
// T must have a default constructor and reset() method
// or be trivially constructible/destructible
template <typename T, size_t InitialCapacity = 64>
class ObjectPool {
public:
    static_assert(std::is_default_constructible_v<T>, "T must be default constructible");

    ObjectPool() {
        // Pre-allocate initial capacity
        for (size_t i = 0; i < InitialCapacity; ++i) {
            available_.push(std::make_unique<T>());
        }
    }

    ~ObjectPool() = default;

    // Disable copy and move
    ObjectPool(const ObjectPool&) = delete;
    ObjectPool& operator=(const ObjectPool&) = delete;

    // Acquire an object from the pool
    [[nodiscard]] std::unique_ptr<T, std::function<void(T*)>> acquire() {
        std::unique_ptr<T> obj;

        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!available_.empty()) {
                obj = std::move(available_.front());
                available_.pop();
            }
        }

        if (!obj) {
            // Pool exhausted, create new object
            obj = std::make_unique<T>();
        }

        // Create deleter that returns object to pool
        auto deleter = [this](T* ptr) {
            if (ptr) {
                reset_object(*ptr);
                std::lock_guard<std::mutex> lock(mutex_);
                available_.push(std::unique_ptr<T>(ptr));
            }
        };

        return std::unique_ptr<T, std::function<void(T*)>>(obj.release(), deleter);
    }

    // Get pool statistics
    [[nodiscard]] size_t available_count() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return available_.size();
    }

    // Clear the pool
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        while (!available_.empty()) {
            available_.pop();
        }
    }

private:
    // Helper to reset object state
    // Uses reset() method if available, otherwise does nothing
    template <typename U = T>
    auto reset_object(U& obj) -> std::void_t<decltype(std::declval<U>().reset())> {
        obj.reset();
    }

    template <typename U = T>
    void reset_object(...) {
        // No reset available, do nothing
    }

    mutable std::mutex mutex_;
    std::queue<std::unique_ptr<T>> available_;
};

// Thread-local object pool for lock-free acquisition
template <typename T, size_t LocalCapacity = 16, size_t SharedCapacity = 64>
class ThreadLocalObjectPool {
public:
    ThreadLocalObjectPool() {
        shared_pool_ = std::make_shared<ObjectPool<T, SharedCapacity>>();
    }

    ~ThreadLocalObjectPool() = default;

    // Disable copy and move
    ThreadLocalObjectPool(const ThreadLocalObjectPool&) = delete;
    ThreadLocalObjectPool& operator=(const ThreadLocalObjectPool&) = delete;

    [[nodiscard]] std::unique_ptr<T, std::function<void(T*)>> acquire() {
        // Try local cache first
        thread_local std::vector<std::unique_ptr<T>> local_cache;

        if (!local_cache.empty()) {
            std::unique_ptr<T> obj = std::move(local_cache.back());
            local_cache.pop_back();

            auto deleter = [this, &local_cache](T* ptr) {
                if (ptr) {
                    reset_object(*ptr);
                    if (local_cache.size() < LocalCapacity) {
                        local_cache.push_back(std::unique_ptr<T>(ptr));
                    } else {
                        // Local cache full, return to shared pool
                        std::lock_guard<std::mutex> lock(shared_pool_->mutex_);
                        shared_pool_->available_.push(std::unique_ptr<T>(ptr));
                    }
                }
            };

            return std::unique_ptr<T, std::function<void(T*)>>(obj.release(), deleter);
        }

        // Fill local cache from shared pool
        {
            std::lock_guard<std::mutex> lock(shared_pool_->mutex_);
            while (local_cache.size() < LocalCapacity && !shared_pool_->available_.empty()) {
                local_cache.push_back(std::move(shared_pool_->available_.front()));
                shared_pool_->available_.pop();
            }
        }

        // Try again with filled cache
        if (!local_cache.empty()) {
            return acquire();
        }

        // Create new object
        std::unique_ptr<T> obj = std::make_unique<T>();

        auto deleter = [this, &local_cache](T* ptr) {
            if (ptr) {
                reset_object(*ptr);
                if (local_cache.size() < LocalCapacity) {
                    local_cache.push_back(std::unique_ptr<T>(ptr));
                } else {
                    std::lock_guard<std::mutex> lock(shared_pool_->mutex_);
                    shared_pool_->available_.push(std::unique_ptr<T>(ptr));
                }
            }
        };

        return std::unique_ptr<T, std::function<void(T*)>>(obj.release(), deleter);
    }

private:
    template <typename U = T>
    auto reset_object(U& obj) -> std::void_t<decltype(std::declval<U>().reset())> {
        obj.reset();
    }

    template <typename U = T>
    void reset_object(...) {
        // No reset available
    }

    struct SharedPool {
        std::mutex mutex_;
        std::queue<std::unique_ptr<T>> available_;
    };

    std::shared_ptr<SharedPool> shared_pool_;
};

} // namespace memory
} // namespace coro
