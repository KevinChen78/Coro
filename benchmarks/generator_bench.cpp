#include <benchmark/benchmark.h>

#include "coro/coro.hpp"

#include <vector>

using namespace coro;

// Benchmark basic generator
static void BM_GeneratorRange(benchmark::State& state) {
    size_t count = state.range(0);

    for (auto _ : state) {
        size_t sum = 0;
        for (auto val : range(count)) {
            sum += val;
        }
        benchmark::DoNotOptimize(sum);
    }
}
BENCHMARK(BM_GeneratorRange)->Range(1, 10000);

// Benchmark generator with transformation
static void BM_GeneratorTransform(benchmark::State& state) {
    size_t count = state.range(0);

    for (auto _ : state) {
        size_t sum = 0;
        auto transformed = transform(range(count), [](size_t x) { return x * 2; });
        for (auto val : transformed) {
            sum += val;
        }
        benchmark::DoNotOptimize(sum);
    }
}
BENCHMARK(BM_GeneratorTransform)->Range(1, 10000);

// Benchmark generator with filter
static void BM_GeneratorFilter(benchmark::State& state) {
    size_t count = state.range(0);

    for (auto _ : state) {
        size_t sum = 0;
        auto filtered = filter(range(count), [](size_t x) { return x % 2 == 0; });
        for (auto val : filtered) {
            sum += val;
        }
        benchmark::DoNotOptimize(sum);
    }
}
BENCHMARK(BM_GeneratorFilter)->Range(1, 10000);

// Benchmark generator take
static void BM_GeneratorTake(benchmark::State& state) {
    size_t take_count = state.range(0);

    for (auto _ : state) {
        size_t sum = 0;
        auto limited = take(iota(0), take_count);
        for (auto val : limited) {
            sum += val;
        }
        benchmark::DoNotOptimize(sum);
    }
}
BENCHMARK(BM_GeneratorTake)->Range(10, 10000);

// Benchmark fibonacci generator
static void BM_FibonacciGenerator(benchmark::State& state) {
    int count = state.range(0);

    for (auto _ : state) {
        auto fib = [](int n) -> Generator<unsigned long long> {
            unsigned long long a = 0, b = 1;
            for (int i = 0; i < n; ++i) {
                co_yield a;
                auto next = a + b;
                a = b;
                b = next;
            }
        }(count);

        unsigned long long sum = 0;
        for (auto val : fib) {
            sum += val;
        }
        benchmark::DoNotOptimize(sum);
    }
}
BENCHMARK(BM_FibonacciGenerator)->Range(10, 100);

// Comparison: generator vs vector for small ranges
static void BM_VectorVsGenerator(benchmark::State& state) {
    size_t count = state.range(0);

    for (auto _ : state) {
        // Generator approach
        size_t gen_sum = 0;
        for (auto val : range(count)) {
            gen_sum += val;
        }
        benchmark::DoNotOptimize(gen_sum);
    }
}
BENCHMARK(BM_VectorVsGenerator)->Range(10, 1000);

static void BM_VectorOnly(benchmark::State& state) {
    size_t count = state.range(0);

    for (auto _ : state) {
        // Vector approach
        std::vector<size_t> vec(count);
        for (size_t i = 0; i < count; ++i) {
            vec[i] = i;
        }
        size_t vec_sum = 0;
        for (auto val : vec) {
            vec_sum += val;
        }
        benchmark::DoNotOptimize(vec_sum);
    }
}
BENCHMARK(BM_VectorOnly)->Range(10, 1000);
