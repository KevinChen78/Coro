#include "coro/coro.hpp"

#include <chrono>
#include <iostream>
#include <vector>

using namespace coro;
using namespace coro::memory;

// Benchmark helper
template <typename Func>
auto benchmark(Func&& func, int iterations = 5) {
    using namespace std::chrono;

    std::vector<double> times;
    times.reserve(iterations);

    for (int i = 0; i < iterations; ++i) {
        auto start = high_resolution_clock::now();
        func();
        auto end = high_resolution_clock::now();
        times.push_back(duration<double, std::micro>(end - start).count());
    }

    // Calculate average and stddev
    double sum = 0;
    for (double t : times) sum += t;
    double avg = sum / iterations;

    double variance = 0;
    for (double t : times) variance += (t - avg) * (t - avg);
    double stddev = std::sqrt(variance / iterations);

    return std::make_pair(avg, stddev);
}

// ==========================================
// Memory Pool Benchmark
// ==========================================

void benchmark_memory_pool() {
    std::cout << "\n=== Memory Pool Benchmark ===" << std::endl;

    constexpr size_t NUM_ALLOCS = 1000000;

    // Standard allocator
    auto [std_avg, std_stddev] = benchmark([=]() {
        std::vector<void*> ptrs;
        ptrs.reserve(NUM_ALLOCS);
        for (size_t i = 0; i < NUM_ALLOCS; ++i) {
            ptrs.push_back(::operator new(64));
        }
        for (void* ptr : ptrs) {
            ::operator delete(ptr);
        }
    });

    std::cout << "Standard allocator: " << std::fixed << std::setprecision(2)
              << std_avg << " us (+/- " << std_stddev << " us)" << std::endl;

    // Memory pool
    MemoryPool<64> pool;
    auto [pool_avg, pool_stddev] = benchmark([=, &pool]() {
        std::vector<void*> ptrs;
        ptrs.reserve(NUM_ALLOCS);
        for (size_t i = 0; i < NUM_ALLOCS; ++i) {
            ptrs.push_back(pool.allocate());
        }
        for (void* ptr : ptrs) {
            pool.deallocate(ptr);
        }
    });

    std::cout << "Memory pool:        " << std::fixed << std::setprecision(2)
              << pool_avg << " us (+/- " << pool_stddev << " us)" << std::endl;
    std::cout << "Speedup:            " << std::fixed << std::setprecision(2)
              << (std_avg / pool_avg) << "x" << std::endl;
}

// ==========================================
// Object Pool Benchmark
// ==========================================

struct TestObject {
    char data[256];
    int id;

    void reset() { id = 0; }
};

void benchmark_object_pool() {
    std::cout << "\n=== Object Pool Benchmark ===" << std::endl;

    constexpr size_t NUM_OBJECTS = 100000;

    // Standard allocation
    auto [std_avg, std_stddev] = benchmark([=]() {
        std::vector<std::unique_ptr<TestObject>> objs;
        objs.reserve(NUM_OBJECTS);
        for (size_t i = 0; i < NUM_OBJECTS; ++i) {
            objs.push_back(std::make_unique<TestObject>());
        }
        objs.clear();
    });

    std::cout << "Standard unique_ptr: " << std::fixed << std::setprecision(2)
              << std_avg << " us (+/- " << std_stddev << " us)" << std::endl;

    // Object pool
    ObjectPool<TestObject, 64> pool;
    auto [pool_avg, pool_stddev] = benchmark([=, &pool]() {
        std::vector<decltype(pool.acquire())> objs;
        objs.reserve(NUM_OBJECTS);
        for (size_t i = 0; i < NUM_OBJECTS; ++i) {
            objs.push_back(pool.acquire());
        }
        objs.clear();
    });

    std::cout << "Object pool:         " << std::fixed << std::setprecision(2)
              << pool_avg << " us (+/- " << pool_stddev << " us)" << std::endl;
    std::cout << "Speedup:             " << std::fixed << std::setprecision(2)
              << (std_avg / pool_avg) << "x" << std::endl;
}

// ==========================================
// Coroutine Creation Benchmark
// ==========================================

Task<int> simple_task(int x) {
    co_return x * 2;
}

void benchmark_coroutine_creation() {
    std::cout << "\n=== Coroutine Creation Benchmark ===" << std::endl;

    constexpr size_t NUM_TASKS = 1000000;

    auto [avg, stddev] = benchmark([=]() {
        std::vector<Task<int>> tasks;
        tasks.reserve(NUM_TASKS);
        for (size_t i = 0; i < NUM_TASKS; ++i) {
            tasks.push_back(simple_task(static_cast<int>(i)));
        }
        for (auto& t : tasks) {
            t.result();
        }
    }, 3);

    std::cout << "Created and executed " << NUM_TASKS << " coroutines" << std::endl;
    std::cout << "Time:  " << std::fixed << std::setprecision(2)
              << avg << " us (+/- " << stddev << " us)" << std::endl;
    std::cout << "Per task: " << std::fixed << std::setprecision(2)
              << (avg / NUM_TASKS * 1000) << " ns" << std::endl;
}

// ==========================================
// Work Stealing Queue Benchmark
// ==========================================

void benchmark_work_stealing() {
    std::cout << "\n=== Work Stealing Queue Benchmark ===" << std::endl;

    constexpr size_t NUM_ITEMS = 1000000;

    auto [avg, stddev] = benchmark([=]() {
        WorkStealQueue<size_t> queue(1024);

        // Push items
        for (size_t i = 0; i < NUM_ITEMS; ++i) {
            queue.push(i);
        }

        // Pop all items
        size_t count = 0;
        while (queue.pop()) {
            ++count;
        }

        if (count != NUM_ITEMS) {
            std::cerr << "Error: expected " << NUM_ITEMS << " items, got " << count << std::endl;
        }
    });

    std::cout << "Pushed and popped " << NUM_ITEMS << " items" << std::endl;
    std::cout << "Time:  " << std::fixed << std::setprecision(2)
              << avg << " us (+/- " << stddev << " us)" << std::endl;
}

// ==========================================
// Thread Pool Benchmark
// ==========================================

Task<void> empty_task() {
    co_return;
}

void benchmark_thread_pool() {
    std::cout << "\n=== Thread Pool Benchmark ===" << std::endl;

    constexpr size_t NUM_TASKS = 100000;

    // Single thread
    {
        auto [avg, stddev] = benchmark([=]() {
            for (size_t i = 0; i < NUM_TASKS; ++i) {
                empty_task().result();
            }
        });

        std::cout << "Single thread:  " << std::fixed << std::setprecision(2)
                  << avg << " us (+/- " << stddev << " us)" << std::endl;
    }

    // Thread pool (commented out - requires proper task scheduling)
    {
        // ThreadPool pool(4);
        // pool.run();
        //
        // auto [avg, stddev] = benchmark([&]() {
        //     // Schedule tasks
        //     for (size_t i = 0; i < NUM_TASKS; ++i) {
        //         auto t = empty_task();
        //         pool.schedule(t.get_handle());
        //     }
        // });
        //
        // std::cout << "Thread pool:    " << std::fixed << std::setprecision(2)
        //           << avg << " us (+/- " << stddev << " us)" << std::endl;
        //
        // pool.stop();
        std::cout << "Thread pool:    (skipped)" << std::endl;
    }
}

// ==========================================
// Channel Benchmark
// ==========================================

void benchmark_channel() {
    std::cout << "\n=== Channel Benchmark ===" << std::endl;

    constexpr size_t NUM_MESSAGES = 100000;

    auto [avg, stddev] = benchmark([=]() {
        Channel<int> ch(1024);

        // Send messages
        for (int i = 0; i < NUM_MESSAGES; ++i) {
            ch.try_send(i);
        }

        // Receive messages
        int count = 0;
        while (ch.try_receive()) {
            ++count;
        }

        if (count != NUM_MESSAGES) {
            std::cerr << "Error: expected " << NUM_MESSAGES << " messages, got " << count << std::endl;
        }
    });

    std::cout << "Sent and received " << NUM_MESSAGES << " messages" << std::endl;
    std::cout << "Time:  " << std::fixed << std::setprecision(2)
              << avg << " us (+/- " << stddev << " us)" << std::endl;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "Coro Library Performance Benchmarks" << std::endl;
    std::cout << "========================================" << std::endl;

    benchmark_memory_pool();
    benchmark_object_pool();
    benchmark_coroutine_creation();
    benchmark_work_stealing();
    // benchmark_thread_pool();  // May have contention issues
    benchmark_channel();

    std::cout << "\n========================================" << std::endl;
    std::cout << "Benchmarks completed!" << std::endl;
    std::cout << "========================================" << std::endl;

    return 0;
}
