#include "coro/scheduler/thread_pool.hpp"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <random>

namespace coro {

// Thread-local storage for current worker ID
thread_local size_t current_worker_id = static_cast<size_t>(-1);

ThreadPool::ThreadPool(size_t num_threads) {
    assert(num_threads > 0 && "Thread pool must have at least one thread");

    workers_.reserve(num_threads);
    threads_.reserve(num_threads);

    for (size_t i = 0; i < num_threads; ++i) {
        workers_.push_back(std::make_unique<Worker>());
    }
}

ThreadPool::~ThreadPool() {
    if (running_) {
        stop();
    }
}

void ThreadPool::schedule(std::coroutine_handle<> handle) {
    if (!handle) return;

    // If called from a worker thread, push to local queue
    if (current_worker_id != static_cast<size_t>(-1)) {
        workers_[current_worker_id]->local_queue.push(handle);
        workers_[current_worker_id]->cv.notify_one();
        return;
    }

    // Called from external thread, use round-robin
    static std::atomic<size_t> next_worker{0};
    size_t worker_id = next_worker++ % workers_.size();

    {
        std::lock_guard<std::mutex> lock(workers_[worker_id]->mutex);
        workers_[worker_id]->local_queue.push(handle);
    }
    workers_[worker_id]->cv.notify_one();
}

void ThreadPool::schedule_on(size_t thread_index, std::coroutine_handle<> handle) {
    if (!handle) return;
    assert(thread_index < workers_.size() && "Invalid thread index");

    {
        std::lock_guard<std::mutex> lock(workers_[thread_index]->mutex);
        workers_[thread_index]->local_queue.push(handle);
    }
    workers_[thread_index]->cv.notify_one();
}

void ThreadPool::run() {
    if (running_.exchange(true)) {
        return;  // Already running
    }

    // Start worker threads
    for (size_t i = 0; i < workers_.size(); ++i) {
        threads_.emplace_back([this, i]() {
            worker_loop(i);
        });
    }
}

void ThreadPool::stop() {
    if (!running_.exchange(false)) {
        return;  // Not running
    }

    // Notify all workers to stop
    for (auto& worker : workers_) {
        worker->running = false;
        worker->cv.notify_all();
    }

    // Wait for all threads to finish
    for (auto& thread : threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }

    threads_.clear();
}

bool ThreadPool::is_on_scheduler_thread() const noexcept {
    return current_worker_id != static_cast<size_t>(-1);
}

size_t ThreadPool::thread_id() const noexcept {
    return current_worker_id;
}

void ThreadPool::yield_current() {
    std::this_thread::yield();
}

void ThreadPool::worker_loop(size_t worker_id) {
    current_worker_id = worker_id;
    Worker& worker = *workers_[worker_id];
    worker.thread_id = std::this_thread::get_id();

    // Set this thread's scheduler
    Scheduler::set_current(this);

    while (worker.running || !worker.local_queue.empty()) {
        auto task_opt = get_task(worker_id);

        if (task_opt) {
            auto handle = *task_opt;
            ++active_tasks_;

            // Resume the coroutine
            handle.resume();

            --active_tasks_;
        } else {
            // No tasks available, wait
            std::unique_lock<std::mutex> lock(worker.mutex);
            worker.cv.wait_for(lock, std::chrono::milliseconds(1),
                [&worker]() { return !worker.local_queue.empty() || !worker.running; });
        }
    }

    // Clear scheduler reference
    Scheduler::set_current(nullptr);
    current_worker_id = static_cast<size_t>(-1);
}

std::optional<std::coroutine_handle<>> ThreadPool::get_task(size_t worker_id) {
    Worker& worker = *workers_[worker_id];

    // Try to pop from local queue first
    auto task = worker.local_queue.pop();
    if (task) {
        return task;
    }

    // Try to steal from other workers
    std::coroutine_handle<> stolen;
    if (steal_task(worker_id, stolen)) {
        return stolen;
    }

    return std::nullopt;
}

bool ThreadPool::steal_task(size_t thief_id, std::coroutine_handle<>& handle) {
    const size_t num_workers = workers_.size();
    if (num_workers <= 1) {
        return false;
    }

    // Random starting point to reduce contention
    static thread_local std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<size_t> dist(0, num_workers - 2);
    size_t start = dist(rng);

    for (size_t i = 0; i < num_workers - 1; ++i) {
        size_t victim_id = (start + i) % num_workers;
        if (victim_id == thief_id) {
            victim_id = (victim_id + 1) % num_workers;
        }

        auto task = workers_[victim_id]->local_queue.steal();
        if (task) {
            handle = *task;
            ++workers_[thief_id]->steal_attempts;
            return true;
        }
    }

    return false;
}

} // namespace coro
