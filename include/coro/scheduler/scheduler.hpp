#pragma once

#include <coroutine>
#include <functional>
#include <memory>

namespace coro {

// Forward declaration
class TaskBase;

// Scheduler interface for coroutine scheduling
class Scheduler {
public:
    virtual ~Scheduler() = default;

    // Schedule a coroutine to be executed
    // This is called when a coroutine needs to be resumed
    virtual void schedule(std::coroutine_handle<> handle) = 0;

    // Run the scheduler (blocks until stop() is called or all work is done)
    virtual void run() = 0;

    // Stop the scheduler
    virtual void stop() = 0;

    // Check if running on this scheduler's thread
    [[nodiscard]] virtual bool is_on_scheduler_thread() const noexcept {
        return true;
    }

    // Get the current scheduler for this thread
    [[nodiscard]] static Scheduler* current() noexcept;

    // Set the current scheduler for this thread
    static void set_current(Scheduler* sched) noexcept;

protected:
    Scheduler() = default;
};

// RAII helper to set/restore current scheduler
class SchedulerScope {
public:
    explicit SchedulerScope(Scheduler& sched)
        : previous_(Scheduler::current()) {
        Scheduler::set_current(&sched);
    }

    ~SchedulerScope() { Scheduler::set_current(previous_); }

    // Disable copy and move
    SchedulerScope(const SchedulerScope&) = delete;
    SchedulerScope& operator=(const SchedulerScope&) = delete;
    SchedulerScope(SchedulerScope&&) = delete;
    SchedulerScope& operator=(SchedulerScope&&) = delete;

private:
    Scheduler* previous_;
};

// Inline scheduler - resumes coroutines immediately
// Useful for testing and synchronous execution
class InlineScheduler : public Scheduler {
public:
    void schedule(std::coroutine_handle<> handle) override {
        if (handle) {
            handle.resume();
        }
    }

    void run() override {
        // Nothing to do - everything is inline
    }

    void stop() override {
        // Nothing to stop
    }
};

// ResumeOn awaiter - allows switching schedulers
class ResumeOnAwaiter {
public:
    explicit ResumeOnAwaiter(Scheduler& target) : target_(target) {}

    [[nodiscard]] bool await_ready() const noexcept { return false; }

    void await_suspend(std::coroutine_handle<> h) {
        target_.schedule(h);
    }

    void await_resume() noexcept {}

private:
    Scheduler& target_;
};

// Helper to switch to a different scheduler
inline ResumeOnAwaiter resume_on(Scheduler& target) {
    return ResumeOnAwaiter{target};
}

// Yield control to the scheduler
class YieldAwaiter {
public:
    [[nodiscard]] bool await_ready() const noexcept { return false; }

    void await_suspend(std::coroutine_handle<> h) {
        if (auto* sched = Scheduler::current()) {
            sched->schedule(h);
        } else {
            // No scheduler, resume inline
            h.resume();
        }
    }

    void await_resume() noexcept {}
};

// Yield control back to the scheduler
inline YieldAwaiter yield() { return YieldAwaiter{}; }

}  // namespace coro