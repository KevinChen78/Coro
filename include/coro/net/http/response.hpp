#pragma once

#include <cstdint>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace coro {
namespace net {
namespace http {

// HTTP status codes
enum class Status : uint16_t {
    OK = 200,
    Created = 201,
    Accepted = 202,
    NoContent = 204,
    MovedPermanently = 301,
    Found = 302,
    NotModified = 304,
    BadRequest = 400,
    Unauthorized = 401,
    Forbidden = 403,
    NotFound = 404,
    MethodNotAllowed = 405,
    Conflict = 409,
    InternalServerError = 500,
    NotImplemented = 501,
    BadGateway = 502,
    ServiceUnavailable = 503
};

// Get status text for status code
inline std::string status_text(Status status) {
    switch (status) {
        case Status::OK: return "OK";
        case Status::Created: return "Created";
        case Status::Accepted: return "Accepted";
        case Status::NoContent: return "No Content";
        case Status::MovedPermanently: return "Moved Permanently";
        case Status::Found: return "Found";
        case Status::NotModified: return "Not Modified";
        case Status::BadRequest: return "Bad Request";
        case Status::Unauthorized: return "Unauthorized";
        case Status::Forbidden: return "Forbidden";
        case Status::NotFound: return "Not Found";
        case Status::MethodNotAllowed: return "Method Not Allowed";
        case Status::Conflict: return "Conflict";
        case Status::InternalServerError: return "Internal Server Error";
        case Status::NotImplemented: return "Not Implemented";
        case Status::BadGateway: return "Bad Gateway";
        case Status::ServiceUnavailable: return "Service Unavailable";
        default: return "Unknown";
    }
}

// HTTP Response builder
class Response {
public:
    Response() = default;

    // Setters
    Response& status(Status code) {
        status_code_ = code;
        return *this;
    }

    Response& version(const std::string& version) {
        version_ = version;
        return *this;
    }

    Response& header(const std::string& name, const std::string& value) {
        headers_[name] = value;
        return *this;
    }

    Response& content_type(const std::string& type) {
        headers_["Content-Type"] = type;
        return *this;
    }

    Response& body(const std::string& content) {
        body_ = content;
        return *this;
    }

    Response& body(const std::vector<uint8_t>& data) {
        body_.assign(reinterpret_cast<const char*>(data.data()), data.size());
        return *this;
    }

    // JSON helpers
    Response& json(const std::string& json_str) {
        content_type("application/json");
        body_ = json_str;
        return *this;
    }

    // HTML helper
    Response& html(const std::string& html_content) {
        content_type("text/html; charset=utf-8");
        body_ = html_content;
        return *this;
    }

    // Plain text helper
    Response& text(const std::string& text_content) {
        content_type("text/plain; charset=utf-8");
        body_ = text_content;
        return *this;
    }

    // Redirect helper
    Response& redirect(const std::string& location, Status code = Status::Found) {
        status(code);
        header("Location", location);
        return *this;
    }

    // Set keep-alive
    Response& keep_alive(bool enabled) {
        if (enabled) {
            headers_["Connection"] = "keep-alive";
        } else {
            headers_["Connection"] = "close";
        }
        return *this;
    }

    // Build response to string
    [[nodiscard]] std::string build() const {
        std::ostringstream oss;

        // Status line
        oss << version_ << " "
            << static_cast<uint16_t>(status_code_) << " "
            << status_text(status_code_) << "\r\n";

        // Headers
        for (const auto& [name, value] : headers_) {
            oss << name << ": " << value << "\r\n";
        }

        // Content-Length (if not already set and body not empty)
        if (headers_.count("Content-Length") == 0 && !body_.empty()) {
            oss << "Content-Length: " << body_.size() << "\r\n";
        }

        // End headers
        oss << "\r\n";

        // Body
        oss << body_;

        return oss.str();
    }

    // Convert to bytes for sending
    [[nodiscard]] std::vector<uint8_t> to_bytes() const {
        std::string str = build();
        return std::vector<uint8_t>(str.begin(), str.end());
    }

    // Getters
    [[nodiscard]] Status status_code() const noexcept { return status_code_; }
    [[nodiscard]] const std::string& get_body() const noexcept { return body_; }

    // Static factory methods for common responses
    static Response ok(const std::string& body = "") {
        return Response().status(Status::OK).body(body);
    }

    static Response not_found(const std::string& message = "Not Found") {
        return Response().status(Status::NotFound).text(message);
    }

    static Response bad_request(const std::string& message = "Bad Request") {
        return Response().status(Status::BadRequest).text(message);
    }

    static Response server_error(const std::string& message = "Internal Server Error") {
        return Response().status(Status::InternalServerError).text(message);
    }

    static Response json_response(const std::string& json) {
        return Response().status(Status::OK).json(json);
    }

private:
    Status status_code_ = Status::OK;
    std::string version_ = "HTTP/1.1";
    std::unordered_map<std::string, std::string> headers_;
    std::string body_;
};

} // namespace http
} // namespace net
} // namespace coro
