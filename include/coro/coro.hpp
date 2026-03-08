#pragma once

// Main header file for coro library

#include "coro/core/task.hpp"
#include "coro/core/generator.hpp"
#include "coro/scheduler/scheduler.hpp"
#include "coro/scheduler/thread_pool.hpp"
#include "coro/sync/mutex.hpp"
#include "coro/sync/semaphore.hpp"
#include "coro/sync/channel.hpp"

// Network components (Phase 4)
#include "coro/io/tcp.hpp"
#include "coro/net/http/request.hpp"
#include "coro/net/http/response.hpp"
#include "coro/net/http/server.hpp"

// Memory management (Phase 5)
#include "coro/memory/pool_allocator.hpp"
#include "coro/memory/coroutine_allocator.hpp"
#include "coro/memory/object_pool.hpp"

// Version information
#define CORO_VERSION_MAJOR 0
#define CORO_VERSION_MINOR 3
#define CORO_VERSION_PATCH 0

namespace coro {

// Library version
[[nodiscard]] inline constexpr const char* version() noexcept {
    return "0.1.0";
}

}  // namespace coro
