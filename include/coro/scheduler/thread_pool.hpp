#pragma once

#include "coro/scheduler/scheduler.hpp"
#include "coro/scheduler/work_steal_queue.hpp"

#include <atomic>
#include <condition_variable>
#include <coroutine>
#include <cstddef>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

namespace coro {

// Thread pool scheduler with work stealing
class ThreadPool : public Scheduler {
public:
    explicit ThreadPool(size_t num_threads = std::thread::hardware_concurrency());
    ~ThreadPool() override;

    // Disable copy and move
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&) = delete;
    ThreadPool& operator=(ThreadPool&&) = delete;

    // Scheduler interface
    void schedule(std::coroutine_handle<> handle) override;
    void run() override;
    void stop() override;
    [[nodiscard]] bool is_on_scheduler_thread() const noexcept override;

    // Thread pool specific
    [[nodiscard]] size_t num_threads() const noexcept { return workers_.size(); }
    [[nodiscard]] size_t thread_id() const noexcept;

    // Schedule with affinity (prefer specific thread)
    void schedule_on(size_t thread_index, std::coroutine_handle<> handle);

    // Yield to allow other tasks to run
    static void yield_current();

private:
    struct Worker {
        WorkStealQueue<std::coroutine_handle<>> local_queue;
        std::atomic<size_t> steal_attempts{0};
        std::atomic<bool> running{true};
        std::mutex mutex;
        std::condition_variable cv;
        std::thread::id thread_id;
    };

    void worker_loop(size_t worker_id);
    std::optional<std::coroutine_handle<>> get_task(size_t worker_id);
    bool steal_task(size_t thief_id, std::coroutine_handle<>& handle);

    std::vector<std::unique_ptr<Worker>> workers_;
    std::vector<std::thread> threads_;

    std::atomic<bool> running_{false};
    std::atomic<size_t> active_tasks_{0};

    // For external thread scheduling
    std::mutex external_mutex_;
    std::vector<std::coroutine_handle<>> external_queue_;
    std::condition_variable external_cv_;
};

// RAII helper to manage thread pool lifecycle
class ThreadPoolExecutor {
public:
    explicit ThreadPoolExecutor(size_t num_threads = std::thread::hardware_concurrency())
        : pool_(num_threads) {
        pool_.run();
    }

    ~ThreadPoolExecutor() {
        pool_.stop();
    }

    [[nodiscard]] ThreadPool& pool() noexcept { return pool_; }

private:
    ThreadPool pool_;
};

} // namespace coro
