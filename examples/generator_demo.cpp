#include "coro/coro.hpp"

#include <iostream>
#include <vector>

using namespace coro;

// Fibonacci generator
coro::Generator<unsigned long long> fibonacci_generator(int count) {
    unsigned long long a = 0, b = 1;
    for (int i = 0; i < count; ++i) {
        co_yield a;
        auto next = a + b;
        a = b;
        b = next;
    }
}

// Prime number generator
coro::Generator<int> primes(int limit) {
    std::vector<bool> is_prime(limit + 1, true);
    is_prime[0] = is_prime[1] = false;

    for (int i = 2; i <= limit; ++i) {
        if (is_prime[i]) {
            co_yield i;
            for (int j = i * 2; j <= limit; j += i) {
                is_prime[j] = false;
            }
        }
    }
}

// File line generator (simulated)
coro::Generator<std::string> read_lines() {
    co_yield "Line 1: Hello World";
    co_yield "Line 2: This is a coroutine";
    co_yield "Line 3: Generators are powerful";
    co_yield "Line 4: Lazy evaluation FTW";
}

// Range with transformation
coro::Generator<int> squares(int n) {
    for (int i = 1; i <= n; ++i) {
        co_yield i * i;
    }
}

int main() {
    std::cout << "=== Coro Generator Demo ===" << std::endl;

    // Fibonacci sequence
    std::cout << "\nFibonacci sequence (first 20):" << std::endl;
    int count = 0;
    for (auto val : fibonacci_generator(20)) {
        std::cout << val << " ";
        if (++count % 10 == 0) std::cout << std::endl;
    }
    std::cout << std::endl;

    // Prime numbers
    std::cout << "\nPrime numbers up to 50:" << std::endl;
    for (auto prime : primes(50)) {
        std::cout << prime << " ";
    }
    std::cout << std::endl;

    // Reading lines
    std::cout << "\nReading lines:" << std::endl;
    for (const auto& line : read_lines()) {
        std::cout << "  " << line << std::endl;
    }

    // Range helpers
    std::cout << "\nRange(10):" << std::endl;
    for (auto i : coro::range(10)) {
        std::cout << i << " ";
    }
    std::cout << std::endl;

    // Transform
    std::cout << "\nSquares of 1-5:" << std::endl;
    for (auto sq : squares(5)) {
        std::cout << sq << " ";
    }
    std::cout << std::endl;

    // Filter example
    std::cout << "\nEven numbers from range(20):" << std::endl;
    auto evens = coro::filter(coro::range(20), [](size_t x) { return x % 2 == 0; });
    for (auto val : evens) {
        std::cout << val << " ";
    }
    std::cout << std::endl;

    // Take example with infinite sequence
    std::cout << "\nFirst 10 elements of iota(100):" << std::endl;
    auto limited = coro::take(coro::iota(100), 10);
    for (auto val : limited) {
        std::cout << val << " ";
    }
    std::cout << std::endl;

    // Iterator style
    std::cout << "\nUsing iterator style:" << std::endl;
    auto gen = fibonacci_generator(5);
    auto it = gen.begin();
    while (it != gen.end()) {
        std::cout << "  " << *it << std::endl;
        ++it;
    }

    std::cout << "\nDemo completed successfully!" << std::endl;
    return 0;
}
