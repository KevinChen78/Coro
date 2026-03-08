#include "coro/coro.hpp"

#include <algorithm>
#include <iostream>
#include <sstream>
#include <string>

using namespace coro;
using namespace coro::net::http;

// JSON helpers for demo responses
std::string json_ok(const std::string& message) {
    return "{\"status\":\"ok\",\"message\":\"" + message + "\"}";
}

std::string json_error(const std::string& message) {
    return "{\"status\":\"error\",\"message\":\"" + message + "\"}";
}

std::string json_data(const std::vector<std::pair<std::string, std::string>>& items) {
    std::ostringstream oss;
    oss << "{";
    for (size_t i = 0; i < items.size(); ++i) {
        if (i > 0) oss << ",";
        oss << "\"" << items[i].first << "\":\"" << items[i].second << "\"";
    }
    oss << "}";
    return oss.str();
}

// ==========================================
// Demo: HTTP Server with Coro
// ==========================================

Task<void> run_http_server() {
    std::cout << "========================================" << std::endl;
    std::cout << "Coro HTTP Server Demo" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;

    // Create thread pool with 4 worker threads
    ThreadPool pool(4);

    // Create HTTP server on port 8080
    Server server(pool, 8080);

    // Define routes
    server
        // GET / - Home page
        .get("/", [](const Request& req, Response& resp) -> Task<void> {
            std::string html = R"(
<!DOCTYPE html>
<html>
<head>
    <title>Coro HTTP Server</title>
    <style>
        body { font-family: Arial, sans-serif; max-width: 800px; margin: 50px auto; padding: 20px; }
        h1 { color: #333; }
        code { background: #f4f4f4; padding: 2px 6px; border-radius: 3px; }
        .endpoint { margin: 10px 0; padding: 10px; background: #f9f9f9; border-left: 3px solid #007acc; }
    </style>
</head>
<body>
    <h1>Coro HTTP Server Demo</h1>
    <p>A C++20 coroutine-based HTTP server built with the <code>coro</code> library.</p>

    <h2>Available Endpoints:</h2>
    <div class="endpoint"><strong>GET /</strong> - This page</div>
    <div class="endpoint"><strong>GET /api/hello</strong> - JSON greeting</div>
    <div class="endpoint"><strong>GET /api/status</strong> - Server status</div>
    <div class="endpoint"><strong>GET /api/echo?message=...</strong> - Echo message</div>
    <div class="endpoint"><strong>POST /api/data</strong> - Submit data</div>
    <div class="endpoint"><strong>GET /api/users/:id</strong> - Get user by ID</div>
</body>
</html>
)";
            resp.html(html);
            co_return;
        })

        // GET /api/hello - JSON greeting
        .get("/api/hello", [](const Request& req, Response& resp) -> Task<void> {
            resp.json(json_ok("Hello from Coro HTTP Server!"));
            co_return;
        })

        // GET /api/status - Server status
        .get("/api/status", [](const Request& req, Response& resp) -> Task<void> {
            std::vector<std::pair<std::string, std::string>> data = {
                {"server", "Coro/0.3.0"},
                {"status", "running"},
                {"coroutines", "enabled"},
                {"thread_pool", "4 workers"}
            };
            resp.json(json_data(data));
            co_return;
        })

        // GET /api/echo - Echo endpoint
        .get("/api/echo", [](const Request& req, Response& resp) -> Task<void> {
            std::string message = req.query_param("message");
            if (message.empty()) {
                message = "No message provided";
            }

            std::vector<std::pair<std::string, std::string>> data = {
                {"echo", message},
                {"method", "GET"},
                {"path", req.path()}
            };
            resp.json(json_data(data));
            co_return;
        })

        // POST /api/data - Data submission
        .post("/api/data", [](const Request& req, Response& resp) -> Task<void> {
            // Parse form data if Content-Type is form-urlencoded
            if (req.has_header("Content-Type") &&
                req.header("Content-Type").find("application/x-www-form-urlencoded") != std::string::npos) {
                // This is a simplified version; in real usage, we'd parse the body
                // const_cast<Request&>(req).parse_form_data();
            }

            std::string received = req.body();
            if (received.empty()) {
                received = "(empty body)";
            } else if (received.length() > 100) {
                received = received.substr(0, 100) + "...";
            }

            std::vector<std::pair<std::string, std::string>> data = {
                {"received", received},
                {"length", std::to_string(req.body().length())},
                {"content_type", req.header("Content-Type")}
            };
            resp.json(json_data(data));
            co_return;
        })

        // GET /api/users/:id - Path parameter example
        .route(Method::GET, "/api/users/", [](const Request& req, Response& resp) -> Task<void> {
            // Extract ID from path /api/users/123
            std::string path = req.path();
            std::string id = path.substr(path.find_last_of('/') + 1);

            if (id.empty() || !std::all_of(id.begin(), id.end(), ::isdigit)) {
                resp.status(Status::BadRequest).json(json_error("Invalid user ID"));
                co_return;
            }

            // Simulate user lookup
            std::vector<std::pair<std::string, std::string>> user = {
                {"id", id},
                {"name", "User " + id},
                {"email", "user" + id + "@example.com"},
                {"status", "active"}
            };

            resp.json(json_data(user));
            co_return;
        })

        // 404 handler for /api/*
        .route(Method::GET, "/api/", [](const Request& req, Response& resp) -> Task<void> {
            resp.status(Status::NotFound).json(json_error("API endpoint not found: " + req.path()));
            co_return;
        });

    // Bind and start server
    if (!server.bind("0.0.0.0")) {
        std::cerr << "Failed to bind to port 8080" << std::endl;
        co_return;
    }

    std::cout << "Server starting on http://0.0.0.0:8080" << std::endl;
    std::cout << "Available routes:" << std::endl;
    for (const auto& route : server.router().list_routes()) {
        std::cout << "  " << route << std::endl;
    }
    std::cout << std::endl;
    std::cout << "Press Ctrl+C to stop the server" << std::endl;
    std::cout << std::endl;

    // Start server in a separate thread
    std::thread server_thread([&server]() {
        server.start();
    });

    // Give the server time to start accepting connections
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Demonstrate server is running
    std::cout << "Server is now accepting connections..." << std::endl;
    std::cout << std::endl;
    std::cout << "Try these commands:" << std::endl;
    std::cout << "  curl http://localhost:8080/" << std::endl;
    std::cout << "  curl http://localhost:8080/api/hello" << std::endl;
    std::cout << "  curl http://localhost:8080/api/status" << std::endl;
    std::cout << "  curl 'http://localhost:8080/api/echo?message=Hello'" << std::endl;
    std::cout << "  curl -X POST -d 'data=test' http://localhost:8080/api/data" << std::endl;
    std::cout << "  curl http://localhost:8080/api/users/123" << std::endl;
    std::cout << std::endl;

    // Run for demonstration (or wait for Ctrl+C in real usage)
    std::cout << "Running server for 30 seconds for demonstration..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(30));

    std::cout << std::endl;
    std::cout << "Shutting down server..." << std::endl;
    server.stop();

    if (server_thread.joinable()) {
        server_thread.join();
    }

    std::cout << "Server stopped." << std::endl;
    co_return;
}

int main() {
    try {
        run_http_server().result();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
