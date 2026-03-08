#pragma once

#include <atomic>
#include <cstddef>
#include <memory>
#include <optional>
#include <vector>

namespace coro {

// Lock-free work-stealing queue based on Chase-Lev algorithm
// This is a single-producer (owner thread), multi-consumer (stealer threads) queue
template <typename T>
class WorkStealQueue {
public:
    explicit WorkStealQueue(size_t initial_capacity = 1024)
        : capacity_(initial_capacity)
        , top_(0)
        , bottom_(0) {
        buffer_.store(new std::atomic<T>[initial_capacity], std::memory_order_relaxed);
    }

    ~WorkStealQueue() {
        delete[] buffer_.load(std::memory_order_relaxed);
    }

    // Disable copy and move
    WorkStealQueue(const WorkStealQueue&) = delete;
    WorkStealQueue& operator=(const WorkStealQueue&) = delete;
    WorkStealQueue(WorkStealQueue&&) = delete;
    WorkStealQueue& operator=(WorkStealQueue&&) = delete;

    // Push an item to the bottom of the queue (owner thread only)
    void push(T item) {
        const size_t b = bottom_.load(std::memory_order_relaxed);
        const size_t t = top_.load(std::memory_order_acquire);

        std::atomic<T>* buffer = buffer_.load(std::memory_order_relaxed);
        const size_t cap = capacity_.load(std::memory_order_relaxed);

        // Check if we need to resize
        if (b - t >= cap - 1) {
            resize();
            buffer = buffer_.load(std::memory_order_relaxed);
        }

        // Store the item
        buffer[b % cap].store(item, std::memory_order_relaxed);

        // Increment bottom (release semantics to ensure item is visible)
        bottom_.store(b + 1, std::memory_order_release);
    }

    // Pop an item from the bottom of the queue (owner thread only)
    std::optional<T> pop() {
        const size_t b = bottom_.load(std::memory_order_relaxed) - 1;
        std::atomic<T>* buffer = buffer_.load(std::memory_order_relaxed);

        bottom_.store(b, std::memory_order_relaxed);

        std::atomic_thread_fence(std::memory_order_seq_cst);

        const size_t t = top_.load(std::memory_order_relaxed);

        if (t <= b) {
            const size_t cap = capacity_.load(std::memory_order_relaxed);
            T item = buffer[b % cap].load(std::memory_order_relaxed);

            if (t == b) {
                // Last item, race with thieves
                size_t expected = t;
                if (!top_.compare_exchange_strong(expected, t + 1,
                        std::memory_order_seq_cst, std::memory_order_relaxed)) {
                    // Lost race, restore bottom
                    bottom_.store(b + 1, std::memory_order_relaxed);
                    return std::nullopt;
                }
                bottom_.store(b + 1, std::memory_order_relaxed);
            }

            return item;
        } else {
            // Queue empty
            bottom_.store(b + 1, std::memory_order_relaxed);
            return std::nullopt;
        }
    }

    // Steal an item from the top of the queue (any thread)
    std::optional<T> steal() {
        const size_t t = top_.load(std::memory_order_acquire);
        std::atomic_thread_fence(std::memory_order_seq_cst);
        const size_t b = bottom_.load(std::memory_order_acquire);

        if (t < b) {
            std::atomic<T>* buffer = buffer_.load(std::memory_order_consume);
            const size_t cap = capacity_.load(std::memory_order_relaxed);
            T item = buffer[t % cap].load(std::memory_order_relaxed);

            size_t expected = t;
            if (top_.compare_exchange_strong(expected, t + 1,
                    std::memory_order_seq_cst, std::memory_order_relaxed)) {
                return item;
            }
            // Failed to steal, another thief got it
            return std::nullopt;
        }

        // Queue empty
        return std::nullopt;
    }

    // Check if queue is empty
    [[nodiscard]] bool empty() const {
        const size_t t = top_.load(std::memory_order_relaxed);
        const size_t b = bottom_.load(std::memory_order_relaxed);
        return t >= b;
    }

    // Get approximate size (may be inaccurate due to concurrent operations)
    [[nodiscard]] size_t size() const {
        const size_t t = top_.load(std::memory_order_relaxed);
        const size_t b = bottom_.load(std::memory_order_relaxed);
        return (b > t) ? (b - t) : 0;
    }

private:
    void resize() {
        const size_t old_cap = capacity_.load(std::memory_order_relaxed);
        const size_t new_cap = old_cap * 2;

        auto* new_buffer = new std::atomic<T>[new_cap];
        std::atomic<T>* old_buffer = buffer_.load(std::memory_order_relaxed);

        const size_t t = top_.load(std::memory_order_relaxed);
        const size_t b = bottom_.load(std::memory_order_relaxed);

        // Copy elements
        for (size_t i = t; i < b; ++i) {
            new_buffer[i % new_cap].store(
                old_buffer[i % old_cap].load(std::memory_order_relaxed),
                std::memory_order_relaxed);
        }

        buffer_.store(new_buffer, std::memory_order_release);
        capacity_.store(new_cap, std::memory_order_release);

        delete[] old_buffer;
    }

    std::atomic<std::atomic<T>*> buffer_;
    std::atomic<size_t> capacity_;
    std::atomic<size_t> top_;      // Only modified by thieves
    std::atomic<size_t> bottom_;   // Only modified by owner
};

} // namespace coro
