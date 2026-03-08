#pragma once

#include "coro/core/task.hpp"

#include <atomic>
#include <coroutine>
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <optional>
#include <queue>
#include <utility>

namespace coro {

// Channel<T> - A simple buffered communication channel for coroutines
// Simplified implementation with buffer only (no waiting)
template <typename T>
class Channel {
public:
    explicit Channel(size_t capacity) : capacity_(capacity), closed_(false) {}

    ~Channel() = default;

    // Disable copy and move
    Channel(const Channel&) = delete;
    Channel& operator=(const Channel&) = delete;
    Channel(Channel&&) = delete;
    Channel& operator=(Channel&&) = delete;

    // Send a value to the channel (non-blocking)
    Task<bool> send(T value) {
        std::lock_guard<std::mutex> lock(mutex_);

        if (closed_) {
            co_return false;
        }

        if (buffer_.size() < capacity_) {
            buffer_.push(std::move(value));
            co_return true;
        }

        // Channel full - in a full implementation, this would suspend
        co_return false;
    }

    // Receive a value from the channel
    Task<std::optional<T>> receive() {
        std::lock_guard<std::mutex> lock(mutex_);

        if (!buffer_.empty()) {
            T value = std::move(buffer_.front());
            buffer_.pop();
            co_return std::move(value);
        }

        if (closed_) {
            co_return std::nullopt;
        }

        // Channel empty - in a full implementation, this would suspend
        co_return std::nullopt;
    }

    // Try to send without blocking
    [[nodiscard]] bool try_send(const T& value) {
        std::lock_guard<std::mutex> lock(mutex_);

        if (closed_) {
            return false;
        }

        if (buffer_.size() < capacity_) {
            buffer_.push(value);
            return true;
        }

        return false;
    }

    // Try to receive without blocking
    [[nodiscard]] std::optional<T> try_receive() {
        std::lock_guard<std::mutex> lock(mutex_);

        if (!buffer_.empty()) {
            T value = std::move(buffer_.front());
            buffer_.pop();
            return value;
        }

        return std::nullopt;
    }

    // Close the channel
    void close() {
        std::lock_guard<std::mutex> lock(mutex_);
        closed_ = true;
    }

    [[nodiscard]] bool is_closed() const noexcept {
        return closed_;
    }

    [[nodiscard]] size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return buffer_.size();
    }

    [[nodiscard]] bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return buffer_.empty();
    }

    [[nodiscard]] size_t capacity() const noexcept {
        return capacity_;
    }

private:
    size_t capacity_;
    std::atomic<bool> closed_;

    mutable std::mutex mutex_;
    std::queue<T> buffer_;
};

} // namespace coro
