#include <gtest/gtest.h>

#include "coro/core/generator.hpp"

#include <vector>

using namespace coro;

// Basic generator test
TEST(GeneratorTest, BasicYield) {
    auto gen = []() -> Generator<int> {
        co_yield 1;
        co_yield 2;
        co_yield 3;
    }();

    std::vector<int> values;
    for (auto val : gen) {
        values.push_back(val);
    }

    EXPECT_EQ(values.size(), 3);
    EXPECT_EQ(values[0], 1);
    EXPECT_EQ(values[1], 2);
    EXPECT_EQ(values[2], 3);
}

// Empty generator
TEST(GeneratorTest, EmptyGenerator) {
    auto gen = []() -> Generator<int> {
        co_return;
    }();

    std::vector<int> values;
    for (auto val : gen) {
        values.push_back(val);
    }

    EXPECT_TRUE(values.empty());
}

// Generator with loop
TEST(GeneratorTest, GeneratorWithLoop) {
    auto gen = []() -> Generator<int> {
        for (int i = 0; i < 5; ++i) {
            co_yield i * i;
        }
    }();

    std::vector<int> values;
    for (auto val : gen) {
        values.push_back(val);
    }

    EXPECT_EQ(values.size(), 5);
    EXPECT_EQ(values[0], 0);
    EXPECT_EQ(values[1], 1);
    EXPECT_EQ(values[2], 4);
    EXPECT_EQ(values[3], 9);
    EXPECT_EQ(values[4], 16);
}

// String generator
TEST(GeneratorTest, StringGenerator) {
    auto gen = []() -> Generator<std::string> {
        co_yield std::string("hello");
        co_yield "world";
    }();

    std::vector<std::string> values;
    for (auto& val : gen) {
        values.push_back(val);
    }

    EXPECT_EQ(values.size(), 2);
    EXPECT_EQ(values[0], "hello");
    EXPECT_EQ(values[1], "world");
}

// Iterator operations
TEST(GeneratorTest, IteratorOperations) {
    auto gen = []() -> Generator<int> {
        co_yield 10;
        co_yield 20;
        co_yield 30;
    }();

    auto it = gen.begin();
    EXPECT_NE(it, gen.end());
    EXPECT_EQ(*it, 10);

    ++it;
    EXPECT_NE(it, gen.end());
    EXPECT_EQ(*it, 20);

    ++it;
    EXPECT_NE(it, gen.end());
    EXPECT_EQ(*it, 30);

    ++it;
    EXPECT_EQ(it, gen.end());
}

// Infinite generator with take
TEST(GeneratorTest, InfiniteGenerator) {
    auto gen = []() -> Generator<int> {
        int i = 0;
        while (true) {
            co_yield i++;
        }
    }();

    auto limited = take(std::move(gen), 5);
    std::vector<int> values;
    for (auto val : limited) {
        values.push_back(val);
    }

    EXPECT_EQ(values.size(), 5);
    EXPECT_EQ(values[0], 0);
    EXPECT_EQ(values[4], 4);
}

// Range helper
TEST(GeneratorTest, RangeHelper) {
    std::vector<size_t> values;
    for (auto val : range(5)) {
        values.push_back(val);
    }

    EXPECT_EQ(values.size(), 5);
    EXPECT_EQ(values[0], 0);
    EXPECT_EQ(values[4], 4);
}

// Range with begin and end
TEST(GeneratorTest, RangeWithBounds) {
    std::vector<size_t> values;
    for (auto val : range(10, 15)) {
        values.push_back(val);
    }

    EXPECT_EQ(values.size(), 5);
    EXPECT_EQ(values[0], 10);
    EXPECT_EQ(values[4], 14);
}

// Transform helper
TEST(GeneratorTest, TransformHelper) {
    auto gen = range(5);
    auto doubled = transform(std::move(gen), [](size_t x) { return x * 2; });

    std::vector<size_t> values;
    for (auto val : doubled) {
        values.push_back(val);
    }

    EXPECT_EQ(values.size(), 5);
    EXPECT_EQ(values[0], 0);
    EXPECT_EQ(values[1], 2);
    EXPECT_EQ(values[4], 8);
}

// Filter helper
TEST(GeneratorTest, FilterHelper) {
    auto gen = range(10);
    auto evens = filter(std::move(gen), [](size_t x) { return x % 2 == 0; });

    std::vector<size_t> values;
    for (auto val : evens) {
        values.push_back(val);
    }

    EXPECT_EQ(values.size(), 5);
    EXPECT_EQ(values[0], 0);
    EXPECT_EQ(values[1], 2);
    EXPECT_EQ(values[4], 8);
}

// Make generator from initializer list
TEST(GeneratorTest, MakeGenerator) {
    auto gen = make_generator({1, 2, 3, 4, 5});

    std::vector<int> values;
    for (auto val : gen) {
        values.push_back(val);
    }

    EXPECT_EQ(values.size(), 5);
    EXPECT_EQ(values[0], 1);
    EXPECT_EQ(values[4], 5);
}

// Move semantics
TEST(GeneratorTest, MoveSemantics) {
    auto gen1 = []() -> Generator<int> {
        co_yield 1;
        co_yield 2;
    }();

    EXPECT_TRUE(gen1);

    auto gen2 = std::move(gen1);
    EXPECT_FALSE(gen1);
    EXPECT_TRUE(gen2);

    std::vector<int> values;
    for (auto val : gen2) {
        values.push_back(val);
    }

    EXPECT_EQ(values.size(), 2);
}

// Next method
TEST(GeneratorTest, NextMethod) {
    auto gen = []() -> Generator<int> {
        co_yield 100;
        co_yield 200;
    }();

    auto val1 = gen.next();
    EXPECT_TRUE(val1.has_value());
    EXPECT_EQ(val1.value(), 100);

    auto val2 = gen.next();
    EXPECT_TRUE(val2.has_value());
    EXPECT_EQ(val2.value(), 200);

    auto val3 = gen.next();
    EXPECT_FALSE(val3.has_value());
}

// Exception in generator
TEST(GeneratorTest, ExceptionInGenerator) {
    auto gen = []() -> Generator<int> {
        co_yield 1;
        throw std::runtime_error("generator error");
    }();

    EXPECT_THROW({
        for (auto val : gen) {
            (void)val;
        }
    }, std::runtime_error);
}
