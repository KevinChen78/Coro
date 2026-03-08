#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <new>
#include <vector>

namespace coro {
namespace memory {

// Fixed-size memory pool for fast allocations
template <size_t BlockSize, size_t BlocksPerChunk = 64>
class MemoryPool {
public:
    static_assert(BlockSize >= sizeof(void*), "BlockSize must be at least sizeof(void*)");

    MemoryPool() noexcept : free_list_(nullptr), chunk_list_(nullptr) {}

    ~MemoryPool() {
        // Free all chunks
        Chunk* chunk = chunk_list_;
        while (chunk) {
            Chunk* next = chunk->next;
            delete chunk;
            chunk = next;
        }
    }

    // Disable copy and move
    MemoryPool(const MemoryPool&) = delete;
    MemoryPool& operator=(const MemoryPool&) = delete;

    [[nodiscard]] void* allocate() {
        // Try free list first (lock-free)
        FreeBlock* block = free_list_.load(std::memory_order_acquire);
        while (block) {
            FreeBlock* next = block->next;
            if (free_list_.compare_exchange_weak(block, next,
                    std::memory_order_release, std::memory_order_relaxed)) {
                return block;
            }
            // CAS failed, retry
        }

        // Allocate from current chunk or new chunk
        std::lock_guard<std::mutex> lock(mutex_);

        // Double-check after acquiring lock
        block = free_list_.load(std::memory_order_relaxed);
        if (block) {
            FreeBlock* next = block->next;
            free_list_.store(next, std::memory_order_relaxed);
            return block;
        }

        // Need new chunk
        Chunk* new_chunk = new Chunk();
        new_chunk->next = chunk_list_;
        chunk_list_ = new_chunk;

        // Add all blocks from new chunk to free list (except first)
        for (size_t i = BlocksPerChunk - 1; i > 0; --i) {
            FreeBlock* fb = reinterpret_cast<FreeBlock*>(new_chunk->data + i * BlockSize);
            fb->next = free_list_.load(std::memory_order_relaxed);
            free_list_.store(fb, std::memory_order_relaxed);
        }

        // Return first block
        return new_chunk->data;
    }

    void deallocate(void* ptr) noexcept {
        if (!ptr) return;

        FreeBlock* block = reinterpret_cast<FreeBlock*>(ptr);
        block->next = free_list_.load(std::memory_order_relaxed);

        // Loop until CAS succeeds
        while (!free_list_.compare_exchange_weak(block->next, block,
                std::memory_order_release, std::memory_order_relaxed)) {
            // Retry with updated next pointer
        }
    }

    [[nodiscard]] size_t block_size() const noexcept { return BlockSize; }

private:
    struct FreeBlock {
        FreeBlock* next;
    };

    struct Chunk {
        alignas(alignof(std::max_align_t)) char data[BlocksPerChunk * BlockSize];
        Chunk* next = nullptr;
    };

    std::atomic<FreeBlock*> free_list_;
    Chunk* chunk_list_;
    std::mutex mutex_;
};

// Size-class based memory pool (multiple fixed-size pools)
class SizeClassPool {
public:
    static constexpr size_t MIN_SIZE = 16;
    static constexpr size_t MAX_SIZE = 4096;
    static constexpr size_t NUM_CLASSES = 8;

    static SizeClassPool& instance() {
        static SizeClassPool inst;
        return inst;
    }

    [[nodiscard]] void* allocate(size_t size) {
        if (size == 0) return nullptr;

        // Size classes: 16, 32, 64, 128, 256, 512, 1024, 4096
        if (size <= 16) return pool16_.allocate();
        if (size <= 32) return pool32_.allocate();
        if (size <= 64) return pool64_.allocate();
        if (size <= 128) return pool128_.allocate();
        if (size <= 256) return pool256_.allocate();
        if (size <= 512) return pool512_.allocate();
        if (size <= 1024) return pool1024_.allocate();
        if (size <= 4096) return pool4096_.allocate();

        return ::operator new(size);
    }

    void deallocate(void* ptr, size_t size) noexcept {
        if (!ptr) return;

        if (size <= 16) pool16_.deallocate(ptr);
        else if (size <= 32) pool32_.deallocate(ptr);
        else if (size <= 64) pool64_.deallocate(ptr);
        else if (size <= 128) pool128_.deallocate(ptr);
        else if (size <= 256) pool256_.deallocate(ptr);
        else if (size <= 512) pool512_.deallocate(ptr);
        else if (size <= 1024) pool1024_.deallocate(ptr);
        else if (size <= 4096) pool4096_.deallocate(ptr);
        else ::operator delete(ptr);
    }

private:
    SizeClassPool() = default;
    ~SizeClassPool() = default;

    MemoryPool<16> pool16_;
    MemoryPool<32> pool32_;
    MemoryPool<64> pool64_;
    MemoryPool<128> pool128_;
    MemoryPool<256> pool256_;
    MemoryPool<512> pool512_;
    MemoryPool<1024> pool1024_;
    MemoryPool<4096> pool4096_;
};

// STL-compatible allocator using the size-class pool
template <typename T>
class PoolAllocator {
public:
    using value_type = T;
    using size_type = size_t;
    using difference_type = ptrdiff_t;
    using propagate_on_container_copy_assignment = std::false_type;
    using propagate_on_container_move_assignment = std::false_type;
    using propagate_on_container_swap = std::false_type;
    using is_always_equal = std::true_type;

    PoolAllocator() noexcept = default;
    ~PoolAllocator() = default;

    template <typename U>
    PoolAllocator(const PoolAllocator<U>&) noexcept {}

    [[nodiscard]] T* allocate(size_t n) {
        size_t bytes = n * sizeof(T);
        return static_cast<T*>(SizeClassPool::instance().allocate(bytes));
    }

    void deallocate(T* ptr, size_t n) noexcept {
        size_t bytes = n * sizeof(T);
        SizeClassPool::instance().deallocate(ptr, bytes);
    }

    // Rebind allocator to type U
    template <typename U>
    struct rebind {
        using other = PoolAllocator<U>;
    };

    template <typename U>
    bool operator==(const PoolAllocator<U>&) const noexcept { return true; }

    template <typename U>
    bool operator!=(const PoolAllocator<U>&) const noexcept { return false; }
};

} // namespace memory
} // namespace coro
