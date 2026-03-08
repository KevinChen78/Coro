#pragma once

#include "coro/memory/pool_allocator.hpp"

#include <cstddef>
#include <cstdlib>
#include <new>

namespace coro {
namespace memory {

// Global coroutine frame pool
// Coroutine frames are typically small and frequently allocated/deallocated
// This pool provides fast allocation for common frame sizes

class CoroutineFramePool {
public:
    static CoroutineFramePool& instance() {
        static CoroutineFramePool inst;
        return inst;
    }

    // Allocate a coroutine frame of given size
    [[nodiscard]] void* allocate(size_t size) {
        // For small frames, use the size-class pool
        if (size <= 4096) {
            return SizeClassPool::instance().allocate(size);
        }
        // Large frames fall back to system allocator
        return ::operator new(size);
    }

    // Deallocate a coroutine frame
    void deallocate(void* ptr, size_t size) noexcept {
        if (!ptr) return;
        if (size <= 4096) {
            SizeClassPool::instance().deallocate(ptr, size);
        } else {
            ::operator delete(ptr);
        }
    }

private:
    CoroutineFramePool() = default;
    ~CoroutineFramePool() = default;
};

// Helper macros for use in promise_type
#define CORO_FRAME_ALLOCATOR                                                  \
    void* operator new(size_t size) {                                           \
        return coro::memory::CoroutineFramePool::instance().allocate(size);     \
    }                                                                           \
    void operator delete(void* ptr, size_t size) {                              \
        coro::memory::CoroutineFramePool::instance().deallocate(ptr, size);     \
    }

// Standalone allocation functions for custom promise types
inline void* allocate_coroutine_frame(size_t size) {
    return CoroutineFramePool::instance().allocate(size);
}

inline void deallocate_coroutine_frame(void* ptr, size_t size) {
    CoroutineFramePool::instance().deallocate(ptr, size);
}

} // namespace memory
} // namespace coro
