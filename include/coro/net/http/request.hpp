#pragma once

#include <cstdint>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace coro {
namespace net {
namespace http {

// HTTP methods
enum class Method {
    GET,
    POST,
    PUT,
    DEL,  // DELETE is a Windows macro
    HEAD,
    OPTIONS,
    PATCH,
    CONNECT,
    TRACE,
    UNKNOWN
};

// Convert method string to enum
inline Method method_from_string(const std::string& str) {
    if (str == "GET") return Method::GET;
    if (str == "POST") return Method::POST;
    if (str == "PUT") return Method::PUT;
    if (str == "DELETE") return Method::DEL;
    if (str == "HEAD") return Method::HEAD;
    if (str == "OPTIONS") return Method::OPTIONS;
    if (str == "PATCH") return Method::PATCH;
    if (str == "CONNECT") return Method::CONNECT;
    if (str == "TRACE") return Method::TRACE;
    return Method::UNKNOWN;
}

// Convert method enum to string
inline std::string method_to_string(Method method) {
    switch (method) {
        case Method::GET: return "GET";
        case Method::POST: return "POST";
        case Method::PUT: return "PUT";
        case Method::DEL: return "DELETE";
        case Method::HEAD: return "HEAD";
        case Method::OPTIONS: return "OPTIONS";
        case Method::PATCH: return "PATCH";
        case Method::CONNECT: return "CONNECT";
        case Method::TRACE: return "TRACE";
        default: return "UNKNOWN";
    }
}

// HTTP Request class
class Request {
public:
    Request() = default;

    // Parse HTTP request from raw bytes
    [[nodiscard]] bool parse(const std::string& data) {
        size_t pos = 0;

        // Parse request line
        std::string line = read_line(data, pos);
        if (line.empty()) return false;

        if (!parse_request_line(line)) return false;

        // Parse headers
        while (pos < data.size()) {
            line = read_line(data, pos);
            if (line.empty()) break;  // Empty line indicates end of headers

            if (!parse_header_line(line)) return false;
        }

        // Rest is body
        if (pos < data.size()) {
            body_ = data.substr(pos);
        }

        return true;
    }

    // Getters
    [[nodiscard]] Method method() const noexcept { return method_; }
    [[nodiscard]] const std::string& path() const noexcept { return path_; }
    [[nodiscard]] const std::string& version() const noexcept { return version_; }
    [[nodiscard]] const std::string& body() const noexcept { return body_; }

    [[nodiscard]] std::string header(const std::string& name) const {
        auto it = headers_.find(to_lower(name));
        if (it != headers_.end()) {
            return it->second;
        }
        return "";
    }

    [[nodiscard]] bool has_header(const std::string& name) const {
        return headers_.count(to_lower(name)) > 0;
    }

    [[nodiscard]] const std::unordered_map<std::string, std::string>& headers() const noexcept {
        return headers_;
    }

    [[nodiscard]] std::string query_param(const std::string& name) const {
        auto it = query_params_.find(name);
        if (it != query_params_.end()) {
            return it->second;
        }
        return "";
    }

    // Content helpers
    [[nodiscard]] size_t content_length() const {
        auto len = header("Content-Length");
        if (!len.empty()) {
            try {
                return static_cast<size_t>(std::stoull(len));
            } catch (...) {
                return 0;
            }
        }
        return 0;
    }

    [[nodiscard]] bool is_keep_alive() const {
        auto conn = header("Connection");
        if (version_ == "HTTP/1.1") {
            return to_lower(conn) != "close";
        }
        return to_lower(conn) == "keep-alive";
    }

    // Parse form data from body (application/x-www-form-urlencoded)
    void parse_form_data() {
        parse_query_string(body_, form_data_);
    }

    [[nodiscard]] std::string form_value(const std::string& name) const {
        auto it = form_data_.find(name);
        if (it != form_data_.end()) {
            return it->second;
        }
        return "";
    }

private:
    [[nodiscard]] std::string read_line(const std::string& data, size_t& pos) const {
        size_t end = data.find("\r\n", pos);
        if (end == std::string::npos) {
            end = data.find('\n', pos);
            if (end == std::string::npos) {
                end = data.size();
            }
        }

        std::string line = data.substr(pos, end - pos);
        pos = end + (data.substr(end, 2) == "\r\n" ? 2 : 1);

        return line;
    }

    [[nodiscard]] bool parse_request_line(const std::string& line) {
        std::istringstream iss(line);
        std::string method_str;

        if (!(iss >> method_str >> path_ >> version_)) {
            return false;
        }

        method_ = method_from_string(method_str);

        // Parse query string from path
        size_t query_pos = path_.find('?');
        if (query_pos != std::string::npos) {
            parse_query_string(path_.substr(query_pos + 1), query_params_);
            path_ = path_.substr(0, query_pos);
        }

        return true;
    }

    [[nodiscard]] bool parse_header_line(const std::string& line) {
        size_t colon_pos = line.find(':');
        if (colon_pos == std::string::npos) return false;

        std::string name = trim(line.substr(0, colon_pos));
        std::string value = trim(line.substr(colon_pos + 1));

        headers_[to_lower(name)] = value;
        return true;
    }

    void parse_query_string(const std::string& query, std::unordered_map<std::string, std::string>& out) {
        std::istringstream iss(query);
        std::string pair;

        while (std::getline(iss, pair, '&')) {
            size_t eq_pos = pair.find('=');
            if (eq_pos != std::string::npos) {
                std::string key = url_decode(pair.substr(0, eq_pos));
                std::string value = url_decode(pair.substr(eq_pos + 1));
                out[key] = value;
            }
        }
    }

    [[nodiscard]] std::string trim(const std::string& str) const {
        size_t start = str.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) return "";
        size_t end = str.find_last_not_of(" \t\r\n");
        return str.substr(start, end - start + 1);
    }

    [[nodiscard]] std::string to_lower(const std::string& str) const {
        std::string result = str;
        for (auto& c : result) {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        return result;
    }

    [[nodiscard]] std::string url_decode(const std::string& encoded) const {
        std::string result;
        result.reserve(encoded.size());

        for (size_t i = 0; i < encoded.size(); ++i) {
            if (encoded[i] == '%' && i + 2 < encoded.size()) {
                int value = 0;
                std::istringstream iss(encoded.substr(i + 1, 2));
                if (iss >> std::hex >> value) {
                    result += static_cast<char>(value);
                    i += 2;
                } else {
                    result += encoded[i];
                }
            } else if (encoded[i] == '+') {
                result += ' ';
            } else {
                result += encoded[i];
            }
        }

        return result;
    }

private:
    Method method_ = Method::UNKNOWN;
    std::string path_;
    std::string version_ = "HTTP/1.1";
    std::unordered_map<std::string, std::string> headers_;
    std::string body_;

    std::unordered_map<std::string, std::string> query_params_;
    std::unordered_map<std::string, std::string> form_data_;
};

} // namespace http
} // namespace net
} // namespace coro
