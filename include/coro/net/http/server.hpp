#pragma once

#include "coro/core/task.hpp"
#include "coro/io/tcp.hpp"
#include "coro/net/http/request.hpp"
#include "coro/net/http/response.hpp"
#include "coro/scheduler/thread_pool.hpp"

#include <atomic>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace coro {
namespace net {
namespace http {

// Handler function type
using Handler = std::function<Task<void>(const Request&, Response&)>;

// Router for matching requests to handlers
class Router {
public:
    Router() = default;

    // Register a route
    Router& route(Method method, const std::string& path, Handler handler) {
        std::string key = make_key(method, path);
        std::lock_guard<std::mutex> lock(mutex_);
        routes_[key] = std::move(handler);
        return *this;
    }

    // Convenience methods
    Router& get(const std::string& path, Handler handler) {
        return route(Method::GET, path, std::move(handler));
    }

    Router& post(const std::string& path, Handler handler) {
        return route(Method::POST, path, std::move(handler));
    }

    Router& put(const std::string& path, Handler handler) {
        return route(Method::PUT, path, std::move(handler));
    }

    Router& del(const std::string& path, Handler handler) {
        return route(Method::DEL, path, std::move(handler));
    }

    // Find handler for request
    [[nodiscard]] Handler find(const Request& req) const {
        std::string key = make_key(req.method(), req.path());

        std::lock_guard<std::mutex> lock(mutex_);
        auto it = routes_.find(key);
        if (it != routes_.end()) {
            return it->second;
        }

        // Try wildcard match for path parameters
        return find_wildcard(req);
    }

    // Add a wildcard route (e.g., "/api/users/:id")
    Router& route_wildcard(Method method, const std::string& pattern, Handler handler) {
        std::lock_guard<std::mutex> lock(mutex_);
        wildcard_routes_.push_back({method, pattern, std::move(handler)});
        return *this;
    }

    // Get all registered routes
    [[nodiscard]] std::vector<std::string> list_routes() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::string> result;
        for (const auto& [key, _] : routes_) {
            result.push_back(key);
        }
        return result;
    }

private:
    [[nodiscard]] std::string make_key(Method method, const std::string& path) const {
        return method_to_string(method) + " " + path;
    }

    [[nodiscard]] Handler find_wildcard(const Request& req) const {
        for (const auto& [method, pattern, handler] : wildcard_routes_) {
            if (method == req.method() && match_pattern(pattern, req.path())) {
                return handler;
            }
        }
        return nullptr;
    }

    [[nodiscard]] bool match_pattern(const std::string& pattern, const std::string& path) const {
        // Simple pattern matching: /api/users/:id matches /api/users/123
        std::vector<std::string> pattern_parts = split(pattern, '/');
        std::vector<std::string> path_parts = split(path, '/');

        if (pattern_parts.size() != path_parts.size()) {
            return false;
        }

        for (size_t i = 0; i < pattern_parts.size(); ++i) {
            if (pattern_parts[i].empty() || pattern_parts[i][0] != ':') {
                if (pattern_parts[i] != path_parts[i]) {
                    return false;
                }
            }
            // :param matches any non-empty segment
            if (path_parts[i].empty()) {
                return false;
            }
        }

        return true;
    }

    [[nodiscard]] std::vector<std::string> split(const std::string& str, char delim) const {
        std::vector<std::string> parts;
        std::istringstream iss(str);
        std::string part;
        while (std::getline(iss, part, delim)) {
            if (!part.empty()) {
                parts.push_back(part);
            }
        }
        return parts;
    }

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, Handler> routes_;
    std::vector<std::tuple<Method, std::string, Handler>> wildcard_routes_;
};

// HTTP Server
class Server {
public:
    explicit Server(ThreadPool& pool, uint16_t port = 8080)
        : pool_(pool)
        , port_(port)
        , running_(false) {
    }

    ~Server() {
        stop();
    }

    // Disable copy
    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;

    // Router access
    [[nodiscard]] Router& router() noexcept { return router_; }

    // Bind to address
    bool bind(const std::string& host = "0.0.0.0") {
        host_ = host;
        return listener_.bind(host, port_);
    }

    // Start the server
    void start() {
        if (!listener_.is_bound() || running_) {
            return;
        }

        running_ = true;
        std::cout << "[HTTP Server] Starting on " << host_ << ":" << port_ << std::endl;

        // Accept loop - this is a simple blocking implementation
        // In production, you'd want to use async I/O with io_uring or similar
        while (running_) {
            auto stream = listener_.accept();
            if (!stream) {
                if (running_) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
                continue;
            }

            // Handle connection in a detached thread
            // Note: In production, use a proper thread pool for connection handling
            std::thread([this, stream = std::move(*stream)]() mutable {
                handle_connection(std::move(stream)).result();
            }).detach();
        }
    }

    // Stop the server
    void stop() {
        running_ = false;
        listener_.stop();
    }

    [[nodiscard]] bool is_running() const noexcept {
        return running_;
    }

    // Convenience methods for routing
    Server& route(Method method, const std::string& path, Handler handler) {
        router_.route(method, path, std::move(handler));
        return *this;
    }

    Server& get(const std::string& path, Handler handler) {
        router_.get(path, std::move(handler));
        return *this;
    }

    Server& post(const std::string& path, Handler handler) {
        router_.post(path, std::move(handler));
        return *this;
    }

    Server& put(const std::string& path, Handler handler) {
        router_.put(path, std::move(handler));
        return *this;
    }

    Server& del(const std::string& path, Handler handler) {
        router_.del(path, std::move(handler));
        return *this;
    }

private:
    [[nodiscard]] Task<void> handle_connection(io::TcpStream stream) {
        // Read HTTP request
        std::string raw_request;
        bool header_complete = false;

        // Read headers
        while (!header_complete) {
            auto line_opt = co_await stream.read_line();
            if (!line_opt) {
                co_return;
            }

            std::string line = *line_opt;
            raw_request += line;
            raw_request += "\r\n";

            if (line.empty()) {
                header_complete = true;
            }
        }

        // Parse request
        Request request;
        if (!request.parse(raw_request)) {
            Response response = Response::bad_request("Invalid HTTP request");
            auto bytes = response.to_bytes();
            co_await stream.write_all(bytes.data(), bytes.size());
            co_return;
        }

        // Read body if Content-Length specified
        size_t content_length = request.content_length();
        if (content_length > 0 && content_length < 10 * 1024 * 1024) {  // 10MB limit
            std::vector<uint8_t> body(content_length);
            bool success = co_await stream.read_exact(body, content_length);
            if (success) {
                raw_request.append(body.begin(), body.end());
                request.parse(raw_request);  // Re-parse with body
            }
        }

        // Find handler
        Response response;
        Handler handler = router_.find(request);

        if (handler) {
            co_await handler(request, response);
        } else {
            response = Response::not_found();
        }

        // Set keep-alive
        response.keep_alive(request.is_keep_alive());

        // Send response
        auto bytes = response.to_bytes();
        co_await stream.write_all(bytes.data(), bytes.size());

        co_return;
    }

private:
    ThreadPool& pool_;
    io::TcpListener listener_;
    Router router_;
    std::string host_;
    uint16_t port_;
    std::atomic<bool> running_;
};

} // namespace http
} // namespace net
} // namespace coro
