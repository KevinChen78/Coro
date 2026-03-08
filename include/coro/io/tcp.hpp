#pragma once

#include "coro/core/task.hpp"
#include "coro/scheduler/scheduler.hpp"

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

// Define ssize_t for Windows
using ssize_t = SSIZE_T;
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace coro {
namespace io {

// Socket wrapper with RAII
class Socket {
public:
    using NativeHandle =
#ifdef _WIN32
        SOCKET;
#else
        int;
#endif

    Socket() noexcept : handle_(invalid_handle()) {}

    explicit Socket(NativeHandle handle) noexcept : handle_(handle) {}

    ~Socket() {
        close();
    }

    // Disable copy
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;

    // Enable move
    Socket(Socket&& other) noexcept : handle_(other.handle_) {
        other.handle_ = invalid_handle();
    }

    Socket& operator=(Socket&& other) noexcept {
        if (this != &other) {
            close();
            handle_ = other.handle_;
            other.handle_ = invalid_handle();
        }
        return *this;
    }

    [[nodiscard]] bool is_valid() const noexcept {
        return handle_ != invalid_handle();
    }

    [[nodiscard]] NativeHandle native_handle() const noexcept {
        return handle_;
    }

    void close() noexcept {
        if (is_valid()) {
#ifdef _WIN32
            ::closesocket(handle_);
#else
            ::close(handle_);
#endif
            handle_ = invalid_handle();
        }
    }

    // Set non-blocking mode
    bool set_non_blocking(bool non_blocking = true) noexcept {
#ifdef _WIN32
        u_long mode = non_blocking ? 1 : 0;
        return ::ioctlsocket(handle_, FIONBIO, &mode) == 0;
#else
        int flags = ::fcntl(handle_, F_GETFL, 0);
        if (flags < 0) return false;
        flags = non_blocking ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK);
        return ::fcntl(handle_, F_SETFL, flags) == 0;
#endif
    }

    static NativeHandle invalid_handle() noexcept {
#ifdef _WIN32
        return INVALID_SOCKET;
#else
        return -1;
#endif
    }

private:
    NativeHandle handle_;
};

// Socket address wrapper
class SocketAddress {
public:
    SocketAddress() = default;

    explicit SocketAddress(const sockaddr_in& addr) : addr_(addr) {}

    static std::optional<SocketAddress> from_string(const std::string& host, uint16_t port) {
        SocketAddress result;
        result.addr_.sin_family = AF_INET;
        result.addr_.sin_port = htons(port);

        if (inet_pton(AF_INET, host.c_str(), &result.addr_.sin_addr) != 1) {
            return std::nullopt;
        }

        return result;
    }

    [[nodiscard]] std::string to_string() const {
        char buffer[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &addr_.sin_addr, buffer, sizeof(buffer));
        return std::string(buffer) + ":" + std::to_string(ntohs(addr_.sin_port));
    }

    [[nodiscard]] const sockaddr* data() const noexcept {
        return reinterpret_cast<const sockaddr*>(&addr_);
    }

    [[nodiscard]] sockaddr* data() noexcept {
        return reinterpret_cast<sockaddr*>(&addr_);
    }

    [[nodiscard]] socklen_t size() const noexcept {
        return sizeof(addr_);
    }

private:
    sockaddr_in addr_{};
};

// WSA initialization helper (Windows only)
#ifdef _WIN32
class WSAInitializer {
public:
    static WSAInitializer& instance() {
        static WSAInitializer inst;
        return inst;
    }

    bool initialized() const noexcept {
        return initialized_;
    }

private:
    WSAInitializer() {
        WSADATA wsaData;
        initialized_ = (::WSAStartup(MAKEWORD(2, 2), &wsaData) == 0);
    }

    ~WSAInitializer() {
        if (initialized_) {
            ::WSACleanup();
        }
    }

    bool initialized_ = false;
};
#endif

// TCP acceptor for incoming connections
class TcpAcceptor {
public:
    TcpAcceptor() = default;

    ~TcpAcceptor() = default;

    // Disable copy
    TcpAcceptor(const TcpAcceptor&) = delete;
    TcpAcceptor& operator=(const TcpAcceptor&) = delete;

    // Enable move
    TcpAcceptor(TcpAcceptor&&) = default;
    TcpAcceptor& operator=(TcpAcceptor&&) = default;

    // Bind to address and start listening
    bool bind(const SocketAddress& addr, int backlog = 128) {
#ifdef _WIN32
        WSAInitializer::instance();
#endif

        Socket::NativeHandle sock = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sock == Socket::invalid_handle()) {
            return false;
        }

        int reuse = 1;
#ifdef _WIN32
        ::setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
                     reinterpret_cast<const char*>(&reuse), sizeof(reuse));
#else
        ::setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
#endif

        if (::bind(sock, addr.data(), addr.size()) != 0) {
#ifdef _WIN32
            ::closesocket(sock);
#else
            ::close(sock);
#endif
            return false;
        }

        if (::listen(sock, backlog) != 0) {
#ifdef _WIN32
            ::closesocket(sock);
#else
            ::close(sock);
#endif
            return false;
        }

        socket_ = Socket(sock);
        return true;
    }

    bool bind(const std::string& host, uint16_t port, int backlog = 128) {
        auto addr = SocketAddress::from_string(host, port);
        if (!addr) return false;
        return bind(*addr, backlog);
    }

    // Accept a connection (blocking)
    [[nodiscard]] std::optional<Socket> accept() {
        sockaddr_in client_addr{};
        socklen_t addr_len = sizeof(client_addr);

        Socket::NativeHandle client = ::accept(
            socket_.native_handle(),
            reinterpret_cast<sockaddr*>(&client_addr),
            &addr_len
        );

        if (client == Socket::invalid_handle()) {
            return std::nullopt;
        }

        return Socket(client);
    }

    // Accept with timeout (milliseconds)
    [[nodiscard]] std::optional<Socket> accept_with_timeout(int timeout_ms) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
#ifdef _WIN32
        FD_SET(socket_.native_handle(), &read_fds);
#else
        FD_SET(socket_.native_handle(), &read_fds);
#endif

        timeval tv{};
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;

        int result = ::select(
            static_cast<int>(socket_.native_handle()) + 1,
            &read_fds, nullptr, nullptr, &tv
        );

        if (result <= 0) {
            return std::nullopt;
        }

        return accept();
    }

    [[nodiscard]] bool is_bound() const noexcept {
        return socket_.is_valid();
    }

    void close() {
        socket_.close();
    }

private:
    Socket socket_;
};

// TCP stream for reading/writing
class TcpStream {
public:
    TcpStream() = default;

    explicit TcpStream(Socket socket) : socket_(std::move(socket)) {}

    ~TcpStream() = default;

    // Disable copy
    TcpStream(const TcpStream&) = delete;
    TcpStream& operator=(const TcpStream&) = delete;

    // Enable move
    TcpStream(TcpStream&&) = default;
    TcpStream& operator=(TcpStream&&) = default;

    // Read data (blocking)
    [[nodiscard]] Task<std::optional<size_t>> read(std::vector<uint8_t>& buffer) {
        if (!socket_.is_valid() || buffer.empty()) {
            co_return std::nullopt;
        }

        ssize_t result = ::recv(
            socket_.native_handle(),
#ifdef _WIN32
            reinterpret_cast<char*>(buffer.data()),
#else
            buffer.data(),
#endif
            static_cast<int>(buffer.size()),
            0
        );

        if (result <= 0) {
            co_return std::nullopt;
        }

        co_return static_cast<size_t>(result);
    }

    // Read exact amount of data
    [[nodiscard]] Task<bool> read_exact(std::vector<uint8_t>& buffer, size_t len) {
        buffer.resize(len);
        size_t total_read = 0;

        while (total_read < len) {
            std::vector<uint8_t> chunk(len - total_read);
            auto result = co_await read(chunk);

            if (!result) {
                co_return false;
            }

            std::copy(chunk.begin(), chunk.begin() + *result, buffer.begin() + total_read);
            total_read += *result;
        }

        co_return true;
    }

    // Read until delimiter
    [[nodiscard]] Task<std::optional<std::string>> read_until(char delimiter) {
        std::string result;
        char ch;

        while (true) {
            ssize_t n = ::recv(
                socket_.native_handle(),
#ifdef _WIN32
                &ch, 1, 0
#else
                &ch, 1, 0
#endif
            );

            if (n <= 0) {
                co_return std::nullopt;
            }

            result.push_back(ch);

            if (ch == delimiter) {
                co_return result;
            }
        }
    }

    // Read line (until \n or \r\n)
    [[nodiscard]] Task<std::optional<std::string>> read_line() {
        std::string result;
        char prev = 0;
        char ch;

        while (true) {
            ssize_t n = ::recv(
                socket_.native_handle(),
#ifdef _WIN32
                &ch, 1, 0
#else
                &ch, 1, 0
#endif
            );

            if (n <= 0) {
                co_return std::nullopt;
            }

            if (ch == '\n') {
                // Remove \r if present
                if (!result.empty() && result.back() == '\r') {
                    result.pop_back();
                }
                co_return result;
            }

            result.push_back(ch);
        }
    }

    // Write data (blocking)
    [[nodiscard]] Task<std::optional<size_t>> write(const uint8_t* data, size_t len) {
        if (!socket_.is_valid() || len == 0) {
            co_return std::nullopt;
        }

        ssize_t result = ::send(
            socket_.native_handle(),
#ifdef _WIN32
            reinterpret_cast<const char*>(data),
#else
            data,
#endif
            static_cast<int>(len),
            0
        );

        if (result <= 0) {
            co_return std::nullopt;
        }

        co_return static_cast<size_t>(result);
    }

    [[nodiscard]] Task<std::optional<size_t>> write(const std::string& str) {
        co_return co_await write(
            reinterpret_cast<const uint8_t*>(str.data()),
            str.size()
        );
    }

    // Write all data
    [[nodiscard]] Task<bool> write_all(const uint8_t* data, size_t len) {
        size_t total_written = 0;

        while (total_written < len) {
            auto result = co_await write(data + total_written, len - total_written);

            if (!result) {
                co_return false;
            }

            total_written += *result;
        }

        co_return true;
    }

    [[nodiscard]] Task<bool> write_all(const std::string& str) {
        co_return co_await write_all(
            reinterpret_cast<const uint8_t*>(str.data()),
            str.size()
        );
    }

    [[nodiscard]] bool is_connected() const noexcept {
        return socket_.is_valid();
    }

    void close() {
        socket_.close();
    }

    [[nodiscard]] Socket release() {
        return std::move(socket_);
    }

private:
    Socket socket_;
};

// TCP listener for accepting connections
class TcpListener {
public:
    TcpListener() = default;

    ~TcpListener() {
        stop();
    }

    // Disable copy
    TcpListener(const TcpListener&) = delete;
    TcpListener& operator=(const TcpListener&) = delete;

    // Enable move
    TcpListener(TcpListener&&) = default;
    TcpListener& operator=(TcpListener&&) = default;

    bool bind(const std::string& host, uint16_t port) {
        return acceptor_.bind(host, port);
    }

    [[nodiscard]] bool is_bound() const noexcept {
        return acceptor_.is_bound();
    }

    // Start accepting connections with a handler
    template<typename Handler>
    void start(Handler&& handler) {
        if (!acceptor_.is_bound()) {
            return;
        }

        running_ = true;

        // Spawn accept loop as a task
        auto accept_loop = [this, handler]() mutable -> Task<void> {
            while (running_) {
                auto client = acceptor_.accept_with_timeout(100);  // 100ms timeout

                if (client) {
                    // Create stream and handle connection
                    TcpStream stream(std::move(*client));

                    // Spawn handler task
                    auto handler_task = [handler, stream = std::move(stream)]() mutable -> Task<void> {
                        co_await handler(std::move(stream));
                        co_return;
                    }();

                    // Fire and forget (in real implementation, schedule to thread pool)
                    handler_task.result();
                }
            }

            co_return;
        };

        // Run the accept loop
        accept_loop().result();
    }

    void stop() {
        running_ = false;
        acceptor_.close();
    }

    [[nodiscard]] bool is_running() const noexcept {
        return running_;
    }

    [[nodiscard]] std::optional<TcpStream> accept() {
        auto socket = acceptor_.accept();
        if (!socket) {
            return std::nullopt;
        }
        return TcpStream(std::move(*socket));
    }

private:
    TcpAcceptor acceptor_;
    std::atomic<bool> running_{false};
};

} // namespace io
} // namespace coro
