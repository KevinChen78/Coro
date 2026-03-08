#include <gtest/gtest.h>

#include "coro/core/task.hpp"
#include "coro/scheduler/scheduler.hpp"

using namespace coro;

// Inline scheduler basic test
TEST(SchedulerTest, InlineScheduler) {
    InlineScheduler sched;

    auto task = []() -> Task<int> {
        co_return 42;
    }();

    EXPECT_TRUE(task.is_ready());
    EXPECT_EQ(task.result(), 42);
}

// Scheduler scope
TEST(SchedulerTest, SchedulerScope) {
    InlineScheduler sched1;
    InlineScheduler sched2;

    EXPECT_EQ(Scheduler::current(), nullptr);

    {
        SchedulerScope scope(sched1);
        EXPECT_EQ(Scheduler::current(), &sched1);

        {
            SchedulerScope scope2(sched2);
            EXPECT_EQ(Scheduler::current(), &sched2);
        }

        EXPECT_EQ(Scheduler::current(), &sched1);
    }

    EXPECT_EQ(Scheduler::current(), nullptr);
}

// Yield awaiter
TEST(SchedulerTest, YieldAwaiter) {
    // When no scheduler is set, yield should resume inline
    bool after_yield = false;

    auto task = [&after_yield]() -> Task<void> {
        co_await yield();
        after_yield = true;
    }();

    // With inline scheduler, task completes immediately
    EXPECT_TRUE(task.is_ready());
    EXPECT_TRUE(after_yield);
}

// Resume on another scheduler
TEST(SchedulerTest, ResumeOn) {
    InlineScheduler sched1;
    InlineScheduler sched2;

    // This test verifies the API compiles correctly
    // Full cross-scheduler execution requires a multi-threaded scheduler
    auto task = [&sched2]() -> Task<int> {
        // Resume on sched2
        co_await resume_on(sched2);
        co_return 123;
    }();

    EXPECT_TRUE(task.is_ready());
}

// Task with scheduler context
TEST(SchedulerTest, TaskWithSchedulerContext) {
    InlineScheduler sched;
    SchedulerScope scope(sched);

    auto task = []() -> Task<int> {
        EXPECT_NE(Scheduler::current(), nullptr);
        co_return 42;
    }();

    EXPECT_EQ(task.result(), 42);
}

// Multiple tasks in same context
TEST(SchedulerTest, MultipleTasksSameContext) {
    InlineScheduler sched;
    SchedulerScope scope(sched);

    std::vector<Task<int>> tasks;
    for (int i = 0; i < 5; ++i) {
        tasks.push_back([i]() -> Task<int> {
            co_return i * 10;
        }());
    }

    for (int i = 0; i < 5; ++i) {
        EXPECT_EQ(tasks[i].result(), i * 10);
    }
}
