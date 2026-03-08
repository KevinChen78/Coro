#include <benchmark/benchmark.h>

#include "coro/coro.hpp"

using namespace coro;

// Benchmark basic Task creation and co_return
static void BM_TaskCreateAndReturn(benchmark::State& state) {
    for (auto _ : state) {
        auto task = []() -> Task<int> {
            co_return 42;
        }();
        benchmark::DoNotOptimize(task.result());
    }
}
BENCHMARK(BM_TaskCreateAndReturn);

// Benchmark chained tasks
static void BM_ChainedTasks(benchmark::State& state) {
    auto inner = []() -> Task<int> {
        co_return 1;
    };

    for (auto _ : state) {
        auto task = [&inner]() -> Task<int> {
            int a = co_await inner();
            int b = co_await inner();
            int c = co_await inner();
            co_return a + b + c;
        }();
        benchmark::DoNotOptimize(task.result());
    }
}
BENCHMARK(BM_ChainedTasks);

// Benchmark Task<void>
static void BM_VoidTask(benchmark::State& state) {
    for (auto _ : state) {
        auto task = []() -> Task<void> {
            co_return;
        }();
        task.result();
    }
}
BENCHMARK(BM_VoidTask);

// Benchmark task move
static void BM_TaskMove(benchmark::State& state) {
    for (auto _ : state) {
        auto task1 = []() -> Task<int> {
            co_return 100;
        }();
        auto task2 = std::move(task1);
        benchmark::DoNotOptimize(task2.result());
    }
}
BENCHMARK(BM_TaskMove);

// Benchmark fibonacci computation
static void BM_Fibonacci(benchmark::State& state) {
    int n = state.range(0);

    for (auto _ : state) {
        auto fib = [](int n) -> Task<int> {
            if (n <= 1) co_return n;
            auto a = co_await [](int n) -> Task<int> {
                if (n <= 1) co_return n;
                auto x = co_await [](int n) -> Task<int> {
                    if (n <= 1) co_return n;
                    co_return n;
                }(n - 1);
                auto y = co_await [](int n) -> Task<int> {
                    if (n <= 1) co_return n;
                    co_return n;
                }(n - 2);
                co_return x + y;
            }(n - 1);
            auto b = co_await [](int n) -> Task<int> {
                if (n <= 1) co_return n;
                co_return n;
            }(n - 2);
            co_return a + b;
        }(n);
        benchmark::DoNotOptimize(fib.result());
    }
}
BENCHMARK(BM_Fibonacci)->Range(1, 10);
