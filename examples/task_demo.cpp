#include "coro/coro.hpp"

#include <iostream>
#include <string>

using namespace coro;

// Simple async computation
Task<int> compute_value(int base) {
    co_return base * 2;
}

// Chained async operations
Task<int> process_data(int input) {
    int step1 = co_await compute_value(input);
    int step2 = co_await compute_value(step1);
    co_return step2;
}

// Void task
Task<void> print_message(std::string msg) {
    std::cout << "Message: " << msg << std::endl;
    co_return;
}

// Fibonacci using coroutines
Task<int> fibonacci(int n) {
    if (n <= 1) {
        co_return n;
    }
    int a = co_await fibonacci(n - 1);
    int b = co_await fibonacci(n - 2);
    co_return a + b;
}

int main() {
    std::cout << "=== Coro Task Demo ===" << std::endl;

    // Simple value return
    auto task1 = compute_value(21);
    std::cout << "compute_value(21) = " << task1.result() << std::endl;

    // Chained operations
    auto task2 = process_data(5);
    std::cout << "process_data(5) = " << task2.result() << std::endl;

    // Void task
    auto task3 = print_message("Hello from coroutine!");
    task3.result();

    // Fibonacci
    for (int i = 0; i <= 10; ++i) {
        auto fib = fibonacci(i);
        std::cout << "fibonacci(" << i << ") = " << fib.result() << std::endl;
    }

    // Error handling
    auto error_task = []() -> Task<int> {
        throw std::runtime_error("Something went wrong!");
        co_return 0;
    }();

    try {
        error_task.result();
    } catch (const std::exception& e) {
        std::cout << "Caught exception: " << e.what() << std::endl;
    }

    std::cout << "\nDemo completed successfully!" << std::endl;
    return 0;
}
