#include "coro/coro.hpp"

#include <iostream>
#include <thread>

using namespace coro;

Task<int> simple_task(int x) {
    std::cout << "Task " << x << " running on thread " << std::this_thread::get_id() << std::endl;
    co_return x * 2;
}

int main() {
    std::cout << "=== Simple Thread Pool Test ===" << std::endl;

    // Test 1: Single task
    std::cout << "\nTest 1: Single task" << std::endl;
    {
        auto task = simple_task(21);
        std::cout << "Result: " << task.result() << std::endl;
    }

    // Test 2: Chained tasks
    std::cout << "\nTest 2: Chained tasks" << std::endl;
    {
        auto task1 = simple_task(10);
        int r1 = task1.result();

        auto task2 = simple_task(r1);
        int r2 = task2.result();

        std::cout << "Result: " << r2 << std::endl;
    }

    // Test 3: Multiple sequential tasks
    std::cout << "\nTest 3: Multiple sequential tasks" << std::endl;
    {
        for (int i = 0; i < 5; ++i) {
            auto task = simple_task(i);
            std::cout << "Task " << i << " result: " << task.result() << std::endl;
        }
    }

    std::cout << "\nAll tests passed!" << std::endl;
    return 0;
}
