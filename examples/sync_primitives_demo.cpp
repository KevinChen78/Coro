#include "coro/coro.hpp"

#include <iostream>
#include <string>
#include <vector>

using namespace coro;

// ==========================================
// Demo 1: AsyncMutex - Coroutine-aware mutex
// ==========================================

Task<void> mutex_demo() {
    std::cout << "\n--- AsyncMutex Demo ---" << std::endl;

    AsyncMutex mutex;
    int shared_counter = 0;

    // Helper task that increments counter
    auto increment_task = [&](int id) -> Task<void> {
        std::cout << "Task " << id << " trying to acquire lock..." << std::endl;

        co_await mutex.lock();
        std::cout << "Task " << id << " acquired lock" << std::endl;

        // Critical section
        int current = shared_counter;
        std::cout << "Task " << id << " read counter: " << current << std::endl;
        shared_counter = current + 1;
        std::cout << "Task " << id << " incremented to: " << shared_counter << std::endl;

        mutex.unlock();
        std::cout << "Task " << id << " released lock" << std::endl;
    };

    // Run multiple tasks
    std::vector<Task<void>> tasks;
    for (int i = 0; i < 3; ++i) {
        tasks.push_back(increment_task(i));
    }

    // Wait for all tasks
    for (auto& task : tasks) {
        task.result();
    }

    std::cout << "Final counter value: " << shared_counter << std::endl;
    co_return;
}

// ==========================================
// Demo 2: Semaphore - Limited concurrency
// ==========================================

Task<void> semaphore_demo() {
    std::cout << "\n--- Semaphore Demo ---" << std::endl;

    // Allow only 2 concurrent operations
    Semaphore semaphore(2);
    int active_count = 0;
    int max_active = 0;

    auto worker_task = [&](int id) -> Task<void> {
        std::cout << "Worker " << id << " waiting for permit..." << std::endl;

        co_await semaphore.acquire();

        // Track active workers
        ++active_count;
        if (active_count > max_active) {
            max_active = active_count;
        }

        std::cout << "Worker " << id << " started (active: " << active_count << ")" << std::endl;

        // Simulate work
        std::cout << "Worker " << id << " working..." << std::endl;

        --active_count;
        std::cout << "Worker " << id << " finished" << std::endl;

        semaphore.release();
    };

    // Launch 5 workers with only 2 permits
    std::vector<Task<void>> workers;
    for (int i = 0; i < 5; ++i) {
        workers.push_back(worker_task(i));
    }

    // Wait for all
    for (auto& w : workers) {
        w.result();
    }

    std::cout << "Max concurrent workers: " << max_active << " (limit: 2)" << std::endl;
    co_return;
}

// ==========================================
// Demo 3: BinarySemaphore - Signaling
// ==========================================

Task<void> binary_semaphore_demo() {
    std::cout << "\n--- BinarySemaphore Demo ---" << std::endl;

    BinarySemaphore ready_signal;
    std::string message;

    // Producer task
    auto producer = [&]() -> Task<void> {
        std::cout << "Producer: Preparing data..." << std::endl;
        message = "Hello from producer!";
        std::cout << "Producer: Signaling ready..." << std::endl;
        ready_signal.release();
        co_return;
    }();

    // Consumer task
    auto consumer = [&]() -> Task<void> {
        std::cout << "Consumer: Waiting for signal..." << std::endl;
        co_await ready_signal.acquire();
        std::cout << "Consumer: Received message: " << message << std::endl;
        co_return;
    }();

    // Note: In real usage with thread pool, these would run concurrently
    producer.result();
    consumer.result();
    co_return;
}

// ==========================================
// Demo 4: Channel - Communication
// ==========================================

Task<void> channel_demo() {
    std::cout << "\n--- Channel Demo ---" << std::endl;

    Channel<int> ch(3);  // Buffered channel with capacity 3

    // Producer
    auto producer = [&]() -> Task<void> {
        for (int i = 1; i <= 5; ++i) {
            std::cout << "Producer: Sending " << i << std::endl;
            bool sent = co_await ch.send(i);
            if (sent) {
                std::cout << "Producer: Sent " << i << " successfully" << std::endl;
            } else {
                std::cout << "Producer: Failed to send " << i << " (channel closed)" << std::endl;
            }
        }
        ch.close();
        std::cout << "Producer: Channel closed" << std::endl;
    }();

    // Consumer
    auto consumer = [&]() -> Task<void> {
        int sum = 0;
        while (true) {
            auto value = co_await ch.receive();
            if (!value) {
                std::cout << "Consumer: Channel closed, exiting" << std::endl;
                break;
            }
            std::cout << "Consumer: Received " << *value << std::endl;
            sum += *value;
        }
        std::cout << "Consumer: Sum = " << sum << std::endl;
    }();

    producer.result();
    consumer.result();
    co_return;
}

// ==========================================
// Demo 5: Try operations (non-blocking)
// ==========================================

Task<void> try_operations_demo() {
    std::cout << "\n--- Try Operations Demo ---" << std::endl;

    AsyncMutex mutex;
    Semaphore sem(1);
    Channel<std::string> ch(2);

    // Try lock
    std::cout << "Trying to acquire mutex..." << std::endl;
    if (mutex.try_lock()) {
        std::cout << "Mutex acquired immediately" << std::endl;
        mutex.unlock();
    } else {
        std::cout << "Mutex busy" << std::endl;
    }

    // Try acquire semaphore
    std::cout << "Trying to acquire semaphore..." << std::endl;
    if (sem.try_acquire()) {
        std::cout << "Semaphore acquired" << std::endl;
        sem.release();
    }

    // Try send/receive on channel
    std::cout << "Trying channel operations..." << std::endl;
    if (ch.try_send("Hello")) {
        std::cout << "Message sent" << std::endl;
    }

    auto msg = ch.try_receive();
    if (msg) {
        std::cout << "Message received: " << *msg << std::endl;
    }
    co_return;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "Coro Sync Primitives Demo" << std::endl;
    std::cout << "========================================" << std::endl;

    // Run all demos
    mutex_demo().result();
    semaphore_demo().result();
    binary_semaphore_demo().result();
    channel_demo().result();
    try_operations_demo().result();

    std::cout << "\n========================================" << std::endl;
    std::cout << "All demos completed!" << std::endl;
    std::cout << "========================================" << std::endl;

    return 0;
}
