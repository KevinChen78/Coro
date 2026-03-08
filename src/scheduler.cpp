#include "coro/scheduler/scheduler.hpp"

#include <thread>
#include <unordered_map>
#include <shared_mutex>

namespace coro {

namespace {
    // Thread-local storage for current scheduler
    thread_local Scheduler* current_scheduler = nullptr;

    // Global registry for debug/tracking purposes (optional)
    std::shared_mutex scheduler_registry_mutex;
    std::unordered_map<std::thread::id, Scheduler*> scheduler_registry;
}

Scheduler* Scheduler::current() noexcept {
    return current_scheduler;
}

void Scheduler::set_current(Scheduler* sched) noexcept {
    current_scheduler = sched;

    // Also update the registry for debugging
    if (sched != nullptr) {
        std::unique_lock lock(scheduler_registry_mutex);
        scheduler_registry[std::this_thread::get_id()] = sched;
    } else {
        std::unique_lock lock(scheduler_registry_mutex);
        scheduler_registry.erase(std::this_thread::get_id());
    }
}

} // namespace coro
