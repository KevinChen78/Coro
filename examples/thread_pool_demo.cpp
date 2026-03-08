#include "coro/coro.hpp"
#include "coro/scheduler/thread_pool.hpp"

#include <chrono>
#include <iostream>
#include <sstream>
#include <thread>
#include <vector>

using namespace coro;
using namespace std::chrono;

// Helper to print with thread ID
void print(const std::string& msg) {
    std::ostringstream oss;
    oss << "[Thread " << std::this_thread::get_id() << "] " << msg << std::endl;
    std::cout << oss.str();
}

// A computationally intensive task
Task<int> compute_square(int n) {
    // Simulate some work
    std::this_thread::sleep_for(milliseconds(10));

    // Print which thread is executing
    std::ostringstream oss;
    oss << "Computing square of " << n;
    print(oss.str());

    co_return n * n;
}

// Parallel map using thread pool
Task<std::vector<int>> parallel_squares(ThreadPool& pool, const std::vector<int>& inputs) {
    std::vector<Task<int>> tasks;

    // Launch all tasks concurrently
    for (int n : inputs) {
        tasks.push_back(compute_square(n));
    }

    // Collect results
    std::vector<int> results;
    for (auto& task : tasks) {
        results.push_back(co_await std::move(task));
    }

    co_return results;
}

// Task that chains multiple operations
Task<int> chained_computation(int start) {
    print("Starting chained computation with " + std::to_string(start));

    int step1 = co_await compute_square(start);
    print("Step 1: square = " + std::to_string(step1));

    int step2 = co_await compute_square(step1);
    print("Step 2: square = " + std::to_string(step2));

    co_return step2;
}

// Demonstrate work stealing
Task<void> work_stealing_demo(ThreadPool& pool) {
    print("Starting work stealing demo");

    // Create many small tasks
    std::vector<Task<int>> tasks;
    for (int i = 0; i < 20; ++i) {
        tasks.push_back(compute_square(i));
    }

    // Await all tasks
    int sum = 0;
    for (auto& task : tasks) {
        sum += co_await std::move(task);
    }

    print("Sum of squares (0-19): " + std::to_string(sum));
}

// Demonstrate scheduler affinity
Task<void> affinity_demo(ThreadPool& pool) {
    print("Starting affinity demo");

    // Get current thread ID
    size_t current_id = pool.thread_id();
    std::ostringstream oss;
    oss << "Running on worker thread " << current_id;
    print(oss.str());

    // Create task that should run on same thread
    auto task = compute_square(42);
    int result = co_await std::move(task);

    print("Result: " + std::to_string(result));
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "Coro Thread Pool Demo" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;

    // Get hardware concurrency
    unsigned int num_threads = std::thread::hardware_concurrency();
    std::cout << "Hardware concurrency: " << num_threads << " threads" << std::endl;
    std::cout << "Using " << num_threads << " threads in pool" << std::endl;
    std::cout << std::endl;

    // Create and start thread pool
    ThreadPool pool(num_threads);

    // Set as current scheduler
    Scheduler::set_current(&pool);

    // Start the pool
    auto start_time = steady_clock::now();
    pool.run();

    // Demo 1: Parallel computation
    std::cout << "--- Demo 1: Parallel Squares ---" << std::endl;
    {
        std::vector<int> inputs = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
        auto task = parallel_squares(pool, inputs);
        auto results = task.result();

        std::cout << "Results: ";
        for (int r : results) {
            std::cout << r << " ";
        }
        std::cout << std::endl;
    }
    std::cout << std::endl;

    // Demo 2: Chained computation
    std::cout << "--- Demo 2: Chained Computation ---" << std::endl;
    {
        auto task = chained_computation(3);
        int result = task.result();
        std::cout << "Final result: " << result << std::endl;
    }
    std::cout << std::endl;

    // Demo 3: Work stealing
    std::cout << "--- Demo 3: Work Stealing (20 tasks) ---" << std::endl;
    {
        auto task = work_stealing_demo(pool);
        task.result();
    }
    std::cout << std::endl;

    // Demo 4: Affinity
    std::cout << "--- Demo 4: Scheduler Affinity ---" << std::endl;
    {
        auto task = affinity_demo(pool);
        task.result();
    }
    std::cout << std::endl;

    auto end_time = steady_clock::now();
    auto duration = duration_cast<milliseconds>(end_time - start_time);

    std::cout << "========================================" << std::endl;
    std::cout << "Total time: " << duration.count() << " ms" << std::endl;
    std::cout << "========================================" << std::endl;

    // Stop the pool
    pool.stop();

    return 0;
}
