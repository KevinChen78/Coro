#pragma once

#include <coroutine>
#include <exception>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace coro {

// Generator<T> - A coroutine that yields values lazily
template <typename T>
class Generator {
public:
    struct Promise {
        Generator get_return_object() {
            return Generator{
                std::coroutine_handle<Promise>::from_promise(*this)};
        }

        std::suspend_always initial_suspend() noexcept { return {}; }

        std::suspend_always final_suspend() noexcept { return {}; }

        void unhandled_exception() {
            exception_ = std::current_exception();
        }

        void return_void() noexcept {}

        template <typename U = T>
            requires std::convertible_to<U, T>
        std::suspend_always yield_value(U&& value) {
            value_.emplace(std::forward<U>(value));
            return {};
        }

        T& value() & {
            rethrow_if_exception();
            return value_.value();
        }

        T&& value() && {
            rethrow_if_exception();
            return std::move(value_.value());
        }

        const T& value() const& {
            rethrow_if_exception();
            return value_.value();
        }

        bool has_value() const noexcept { return value_.has_value(); }

        void clear_value() noexcept { value_.reset(); }

        void rethrow_if_exception() const {
            if (exception_) {
                std::rethrow_exception(exception_);
            }
        }

    private:
        std::optional<T> value_;
        std::exception_ptr exception_;
    };

    using promise_type = Promise;
    using value_type = T;

    // Iterator for range-based for loops
    class Iterator {
    public:
        using iterator_category = std::input_iterator_tag;
        using value_type = T;
        using difference_type = std::ptrdiff_t;
        using pointer = T*;
        using reference = T&;

        Iterator() = default;

        explicit Iterator(std::coroutine_handle<Promise> handle)
            : handle_(handle) {}

        Iterator& operator++() {
            if (handle_) {
                handle_.promise().clear_value();
                handle_.resume();
                if (handle_.done()) {
                    handle_.promise().rethrow_if_exception();
                    handle_ = nullptr;
                }
            }
            return *this;
        }

        // Post-increment
        Iterator operator++(int) {
            Iterator tmp = *this;
            ++(*this);
            return tmp;
        }

        T& operator*() const {
            if (!handle_) {
                throw std::runtime_error("Dereferencing invalid generator iterator");
            }
            return handle_.promise().value();
        }

        T* operator->() const {
            if (!handle_) {
                throw std::runtime_error("Dereferencing invalid generator iterator");
            }
            return &handle_.promise().value();
        }

        [[nodiscard]] bool operator==(const Iterator& other) const noexcept {
            return handle_ == other.handle_;
        }

        [[nodiscard]] bool operator!=(const Iterator& other) const noexcept {
            return !(*this == other);
        }

    private:
        std::coroutine_handle<Promise> handle_;
    };

    Generator() = default;

    explicit Generator(std::coroutine_handle<Promise> handle) : handle_(handle) {}

    ~Generator() {
        if (handle_) {
            handle_.destroy();
        }
    }

    // Disable copy
    Generator(const Generator&) = delete;
    Generator& operator=(const Generator&) = delete;

    // Enable move
    Generator(Generator&& other) noexcept
        : handle_(std::exchange(other.handle_, {})) {}

    Generator& operator=(Generator&& other) noexcept {
        if (this != &other) {
            if (handle_) {
                handle_.destroy();
            }
            handle_ = std::exchange(other.handle_, {});
        }
        return *this;
    }

    // Range support
    Iterator begin() {
        if (!handle_) {
            return Iterator{};
        }
        // Prime the generator
        handle_.resume();
        if (handle_.done()) {
            handle_.promise().rethrow_if_exception();
            return Iterator{};
        }
        return Iterator{handle_};
    }

    Iterator end() noexcept { return Iterator{}; }

    // Get next value (returns nullopt if done)
    [[nodiscard]] std::optional<T> next() {
        if (!handle_ || handle_.done()) {
            return std::nullopt;
        }

        // Resume and get value
        handle_.resume();

        if (handle_.done()) {
            handle_.promise().rethrow_if_exception();
            return std::nullopt;
        }

        return handle_.promise().value();
    }

    // Check if generator is done
    [[nodiscard]] bool done() const noexcept {
        return !handle_ || handle_.done();
    }

    // Get the underlying handle
    [[nodiscard]] std::coroutine_handle<Promise> handle() const noexcept {
        return handle_;
    }

    // Check if valid
    [[nodiscard]] explicit operator bool() const noexcept {
        return handle_ != nullptr;
    }

private:
    std::coroutine_handle<Promise> handle_;
};

// Helper to create a generator from an initializer list
template <typename T>
Generator<T> make_generator(std::initializer_list<T> values) {
    for (const auto& v : values) {
        co_yield v;
    }
}

// Helper to generate a sequence of integers
Generator<std::size_t> iota(std::size_t start = 0, std::size_t step = 1) {
    while (true) {
        co_yield start;
        start += step;
    }
}

// Helper to generate a range of integers [begin, end)
Generator<std::size_t> range(std::size_t begin, std::size_t end,
                             std::size_t step = 1) {
    for (auto i = begin; i < end; i += step) {
        co_yield i;
    }
}

// Helper to generate a range of integers [0, end)
Generator<std::size_t> range(std::size_t end) { return range(0, end); }

// Transform a generator
template <typename T, typename F>
auto transform(Generator<T> gen, F&& func)
    -> Generator<std::invoke_result_t<F, T>> {
    for (auto&& value : gen) {
        co_yield func(std::forward<decltype(value)>(value));
    }
}

// Filter a generator
template <typename T, typename F>
Generator<T> filter(Generator<T> gen, F&& predicate) {
    for (auto&& value : gen) {
        if (predicate(value)) {
            co_yield std::forward<decltype(value)>(value);
        }
    }
}

// Take n elements from a generator
template <typename T>
Generator<T> take(Generator<T> gen, std::size_t n) {
    std::size_t count = 0;
    for (auto&& value : gen) {
        if (count >= n) break;
        co_yield std::forward<decltype(value)>(value);
        ++count;
    }
}

// Zip two generators together (requires C++23 zip or custom implementation)
// For now, provide a simple version that returns pairs
template <typename T, typename U>
Generator<std::pair<T, U>> zip(Generator<T> gen1, Generator<U> gen2) {
    auto it1 = gen1.begin();
    auto it2 = gen2.begin();

    while (it1 != gen1.end() && it2 != gen2.end()) {
        co_yield {*it1, *it2};
        ++it1;
        ++it2;
    }
}

}  // namespace coro
