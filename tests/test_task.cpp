#include <gtest/gtest.h>

#include "coro/core/task.hpp"

using namespace coro;

// Basic task creation and co_return
TEST(TaskTest, BasicCoReturn) {
    auto task = []() -> Task<int> {
        co_return 42;
    }();

    EXPECT_TRUE(task.is_ready());
    EXPECT_EQ(task.result(), 42);
}

// Task<void> basic test
TEST(TaskTest, VoidTask) {
    bool executed = false;
    auto task = [&executed]() -> Task<void> {
        executed = true;
        co_return;
    }();

    EXPECT_TRUE(task.is_ready());
    task.result();
    EXPECT_TRUE(executed);
}

// Chained tasks
TEST(TaskTest, ChainedTasks) {
    auto inner = []() -> Task<int> {
        co_return 10;
    };

    auto outer = [&inner]() -> Task<int> {
        int val = co_await inner();
        co_return val * 2;
    };

    auto task = outer();
    EXPECT_TRUE(task.is_ready());
    EXPECT_EQ(task.result(), 20);
}

// Multiple awaits
TEST(TaskTest, MultipleAwaits) {
    auto make_task = [](int val) -> Task<int> {
        co_return val;
    };

    auto task = [&make_task]() -> Task<int> {
        int a = co_await make_task(10);
        int b = co_await make_task(20);
        int c = co_await make_task(30);
        co_return a + b + c;
    }();

    EXPECT_TRUE(task.is_ready());
    EXPECT_EQ(task.result(), 60);
}

// Task move semantics
TEST(TaskTest, MoveSemantics) {
    auto task1 = []() -> Task<int> {
        co_return 100;
    }();

    EXPECT_TRUE(task1);
    auto task2 = std::move(task1);
    EXPECT_FALSE(task1);  // NOLINT: task1 is in moved-from state
    EXPECT_TRUE(task2);
    EXPECT_EQ(task2.result(), 100);
}

// Exception propagation
TEST(TaskTest, ExceptionPropagation) {
    auto task = []() -> Task<int> {
        throw std::runtime_error("test error");
        co_return 0;
    }();

    EXPECT_TRUE(task.is_ready());
    EXPECT_THROW(task.result(), std::runtime_error);
}

// Exception in chained task
TEST(TaskTest, ExceptionInChainedTask) {
    auto inner = []() -> Task<int> {
        throw std::logic_error("inner error");
        co_return 0;
    };

    auto outer = [&inner]() -> Task<int> {
        try {
            int val = co_await inner();
            co_return val;
        } catch (const std::logic_error& e) {
            co_return -1;
        }
    }();

    EXPECT_TRUE(outer.is_ready());
    EXPECT_EQ(outer.result(), -1);
}

// make_task helper
TEST(TaskTest, MakeTaskHelper) {
    auto task = make_task(42);
    EXPECT_TRUE(task.is_ready());
    EXPECT_EQ(task.result(), 42);

    auto void_task = make_task();
    EXPECT_TRUE(void_task.is_ready());
}

// Task with string
TEST(TaskTest, StringTask) {
    auto task = []() -> Task<std::string> {
        co_return "hello coro";
    }();

    EXPECT_TRUE(task.is_ready());
    EXPECT_EQ(task.result(), "hello coro");
}

// Nested coroutine calls
TEST(TaskTest, NestedCoroutines) {
    auto level3 = []() -> Task<int> {
        co_return 3;
    };

    auto level2 = [&level3]() -> Task<int> {
        int v = co_await level3();
        co_return v * 10;
    };

    auto level1 = [&level2]() -> Task<int> {
        int v = co_await level2();
        co_return v * 10;
    }();

    EXPECT_TRUE(level1.is_ready());
    EXPECT_EQ(level1.result(), 300);
}

// Task that returns reference type (should work with copy)
TEST(TaskTest, LargeObjectTask) {
    auto task = []() -> Task<std::vector<int>> {
        std::vector<int> vec(1000, 42);
        co_return vec;
    }();

    EXPECT_TRUE(task.is_ready());
    auto result = task.result();
    EXPECT_EQ(result.size(), 1000);
    EXPECT_EQ(result[0], 42);
}
