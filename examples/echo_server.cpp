#include "coro/coro.hpp"

#include <iostream>
#include <string>

using namespace coro;

// This is a placeholder echo server using coroutines
// In a full implementation, this would use actual TCP sockets

// Simulated async read operation
coro::Task<std::string> async_read() {
    // In real implementation, this would await on socket read
    co_return "Hello from client!";
}

// Simulated async write operation
coro::Task<void> async_write(const std::string& data) {
    std::cout << "Server response: " << data << std::endl;
    co_return;
}

// Handle a single client connection
coro::Task<void> handle_client(int client_id) {
    std::cout << "Client " << client_id << " connected" << std::endl;

    // Read request
    std::string request = co_await async_read();
    std::cout << "Client " << client_id << " sent: " << request << std::endl;

    // Echo back
    std::string response = "Echo: " + request;
    co_await async_write(response);

    std::cout << "Client " << client_id << " disconnected" << std::endl;
}

// Simulated server that accepts connections
coro::Task<void> run_server(int num_clients) {
    std::cout << "=== Coro Echo Server Demo ===" << std::endl;
    std::cout << "Server listening on port 8080..." << std::endl;

    // Accept and handle multiple clients
    std::vector<coro::Task<void>> client_tasks;
    for (int i = 0; i < num_clients; ++i) {
        client_tasks.push_back(handle_client(i));
    }

    // Wait for all clients to complete
    for (auto& task : client_tasks) {
        co_await std::move(task);
    }

    std::cout << "Server shutting down..." << std::endl;
}

int main() {
    try {
        auto server = run_server(3);
        server.result();

        std::cout << "\nNote: This is a simulated echo server." << std::endl;
        std::cout << "In the full implementation, this would use actual TCP sockets." << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
