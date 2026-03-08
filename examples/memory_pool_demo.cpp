#include "coro/coro.hpp"

#include <chrono>
#include <iostream>
#include <vector>

using namespace coro;
using namespace coro::memory;

// ==========================================
// Demo 1: Memory Pool Allocation
// ==========================================

void pool_allocation_demo() {
    std::cout << "\n--- Memory Pool Allocation Demo ---" << std::endl;

    constexpr size_t NUM_ALLOCS = 100000;
    constexpr size_t BLOCK_SIZE = 64;

    // Test standard allocator
    auto start = std::chrono::high_resolution_clock::now();
    std::vector<void*> std_ptrs;
    std_ptrs.reserve(NUM_ALLOCS);

    for (size_t i = 0; i < NUM_ALLOCS; ++i) {
        void* ptr = ::operator new(BLOCK_SIZE);
        std_ptrs.push_back(ptr);
    }
    for (void* ptr : std_ptrs) {
        ::operator delete(ptr);
    }

    auto std_time = std::chrono::high_resolution_clock::now() - start;
    auto std_us = std::chrono::duration_cast<std::chrono::microseconds>(std_time).count();

    // Test memory pool
    start = std::chrono::high_resolution_clock::now();
    MemoryPool<BLOCK_SIZE> pool;
    std::vector<void*> pool_ptrs;
    pool_ptrs.reserve(NUM_ALLOCS);

    for (size_t i = 0; i < NUM_ALLOCS; ++i) {
        void* ptr = pool.allocate();
        pool_ptrs.push_back(ptr);
    }
    for (void* ptr : pool_ptrs) {
        pool.deallocate(ptr);
    }

    auto pool_time = std::chrono::high_resolution_clock::now() - start;
    auto pool_us = std::chrono::duration_cast<std::chrono::microseconds>(pool_time).count();

    std::cout << "Standard allocator: " << std_us << " us" << std::endl;
    std::cout << "Memory pool:        " << pool_us << " us" << std::endl;
    std::cout << "Speedup:            " << (double)std_us / pool_us << "x" << std::endl;
}

// ==========================================
// Demo 2: Object Pool
// ==========================================

struct HttpRequest {
    std::string method;
    std::string path;
    std::string body;
    int id = 0;

    void reset() {
        method.clear();
        path.clear();
        body.clear();
        id = 0;
    }
};

void object_pool_demo() {
    std::cout << "\n--- Object Pool Demo ---" << std::endl;

    ObjectPool<HttpRequest, 32> request_pool;

    std::cout << "Initial available: " << request_pool.available_count() << std::endl;

    {
        auto req1 = request_pool.acquire();
        req1->method = "GET";
        req1->path = "/api/users";
        req1->id = 1;

        auto req2 = request_pool.acquire();
        req2->method = "POST";
        req2->path = "/api/data";
        req2->id = 2;

        std::cout << "After acquire 2: " << request_pool.available_count() << std::endl;

        std::cout << "Request 1: " << req1->method << " " << req1->path << std::endl;
        std::cout << "Request 2: " << req2->method << " " << req2->path << std::endl;

        // Objects are automatically returned to pool when going out of scope
    }

    std::cout << "After release: " << request_pool.available_count() << std::endl;

    // Acquire again - should reuse pooled objects
    {
        auto req3 = request_pool.acquire();
        std::cout << "Request 3 (reused): id=" << req3->id << " (should be 0, reset was called)" << std::endl;
    }
}

// ==========================================
// Demo 3: Pool Allocator with STL containers
// ==========================================

void pool_allocator_demo() {
    std::cout << "\n--- Pool Allocator Demo ---" << std::endl;

    // Use pool allocator with vector
    std::vector<int, PoolAllocator<int>> vec;
    vec.reserve(100);

    for (int i = 0; i < 100; ++i) {
        vec.push_back(i);
    }

    std::cout << "Vector with pool allocator: " << vec.size() << " elements" << std::endl;

    // Use pool allocator with string
    std::basic_string<char, std::char_traits<char>, PoolAllocator<char>> str;
    str = "Hello from pool-allocated string!";
    std::cout << "String: " << str << std::endl;
}

// ==========================================
// Demo 4: Coroutine Frame Pool
// ==========================================

Task<int> pooled_coroutine(int value) {
    // This coroutine uses the pooled frame allocator
    // if the promise_type uses CORO_FRAME_ALLOCATOR
    co_return value * 2;
}

Task<void> coroutine_pool_demo() {
    std::cout << "\n--- Coroutine Frame Pool Demo ---" << std::endl;

    constexpr int NUM_TASKS = 10000;

    auto start = std::chrono::high_resolution_clock::now();

    std::vector<Task<int>> tasks;
    tasks.reserve(NUM_TASKS);

    for (int i = 0; i < NUM_TASKS; ++i) {
        tasks.push_back(pooled_coroutine(i));
    }

    // Wait for all tasks
    int sum = 0;
    for (auto& task : tasks) {
        sum += task.result();
    }

    auto elapsed = std::chrono::high_resolution_clock::now() - start;
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();

    std::cout << "Created and ran " << NUM_TASKS << " coroutines in " << ms << " ms" << std::endl;
    std::cout << "Sum of results: " << sum << std::endl;

    co_return;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "Coro Memory Pool Demo" << std::endl;
    std::cout << "========================================" << std::endl;

    pool_allocation_demo();
    object_pool_demo();
    pool_allocator_demo();
    coroutine_pool_demo().result();

    std::cout << "\n========================================" << std::endl;
    std::cout << "All demos completed!" << std::endl;
    std::cout << "========================================" << std::endl;

    return 0;
}
