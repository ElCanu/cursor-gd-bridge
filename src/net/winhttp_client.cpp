#include "net/winhttp_client.hpp"

#include "config/defaults.hpp"

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>
#include <winhttp.h>

#include <format>
#include <stdexcept>
#include <string>
#include <utility>

namespace bridge::net {
namespace {

struct HttpHandle {
    HINTERNET value = nullptr;

    HttpHandle() = default;
    explicit HttpHandle(HINTERNET handle) : value(handle) {}
    ~HttpHandle() {
        if (value != nullptr) {
            WinHttpCloseHandle(value);
        }
    }

    HttpHandle(const HttpHandle&) = delete;
    HttpHandle& operator=(const HttpHandle&) = delete;

    HttpHandle(HttpHandle&& other) noexcept : value(other.value) { other.value = nullptr; }
    HttpHandle& operator=(HttpHandle&& other) noexcept {
        if (this != &other) {
            if (value != nullptr) {
                WinHttpCloseHandle(value);
            }
            value = other.value;
            other.value = nullptr;
        }
        return *this;
    }
};

[[nodiscard]] std::string Win32Message(std::string_view what) {
    return std::format("{} (win32 {})", what, GetLastError());
}

[[nodiscard]] std::expected<std::string, std::string> ReadBody(HINTERNET request) {
    std::string body;
    body.reserve(4096);
    for (;;) {
        DWORD available = 0;
        if (!WinHttpQueryDataAvailable(request, &available)) {
            return std::unexpected(Win32Message("WinHttpQueryDataAvailable"));
        }
        if (available == 0) {
            return body;
        }
        const DWORD chunk = available > 16 * 1024 ? 16 * 1024 : available;
        std::string block(static_cast<std::size_t>(chunk), '\0');
        DWORD read = 0;
        if (!WinHttpReadData(request, block.data(), chunk, &read)) {
            return std::unexpected(Win32Message("WinHttpReadData"));
        }
        block.resize(read);
        body += block;
    }
}

}  // namespace

struct WinHttpClient::Handles {
    HttpHandle session;
    HttpHandle connection;
};

// Session + connection stay open for the process so TLS and keep-alive are reused.
WinHttpClient::WinHttpClient() {
    handles_ = new Handles();
    HINTERNET session = WinHttpOpen(
        L"cursor-gd-bridge/0.1",
        WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0);
    if (session == nullptr) {
        session = WinHttpOpen(
            L"cursor-gd-bridge/0.1",
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
            WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS,
            0);
    }
    if (session == nullptr) {
        delete handles_;
        handles_ = nullptr;
        throw std::runtime_error(Win32Message("WinHttpOpen"));
    }
    handles_->session = HttpHandle(session);
    WinHttpSetTimeouts(
        session,
        defaults::kHttpResolveMs,
        defaults::kHttpConnectMs,
        defaults::kHttpSendMs,
        defaults::kHttpReceiveMs);

    HINTERNET connection = WinHttpConnect(
        session,
        std::wstring(defaults::kCursorHost).c_str(),
        INTERNET_DEFAULT_HTTPS_PORT,
        0);
    if (connection == nullptr) {
        delete handles_;
        handles_ = nullptr;
        throw std::runtime_error(Win32Message("WinHttpConnect"));
    }
    handles_->connection = HttpHandle(connection);
}

WinHttpClient::~WinHttpClient() {
    delete handles_;
    handles_ = nullptr;
}

std::expected<HttpResponse, std::string> WinHttpClient::Request(
    std::wstring_view method,
    std::wstring_view path,
    std::string_view body,
    std::wstring_view content_type,
    std::wstring_view authorization) {
    if (handles_ == nullptr || handles_->connection.value == nullptr) {
        return std::unexpected("WinHTTP session is not open");
    }
    if (body.size() > static_cast<std::size_t>(MAXDWORD)) {
        return std::unexpected("request body is too large");
    }

    const std::wstring method_owned(method);
    const std::wstring path_owned(path);
    HttpHandle request(WinHttpOpenRequest(
        handles_->connection.value,
        method_owned.c_str(),
        path_owned.c_str(),
        nullptr,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        WINHTTP_FLAG_SECURE));
    if (request.value == nullptr) {
        return std::unexpected(Win32Message("WinHttpOpenRequest"));
    }

    DWORD decompression = WINHTTP_DECOMPRESSION_FLAG_GZIP | WINHTTP_DECOMPRESSION_FLAG_DEFLATE;
    WinHttpSetOption(request.value, WINHTTP_OPTION_DECOMPRESSION, &decompression, sizeof(decompression));

    auto add_header = [&](std::wstring_view header) -> std::expected<void, std::string> {
        const std::wstring line(header);
        if (!WinHttpAddRequestHeaders(
                request.value,
                line.c_str(),
                static_cast<DWORD>(-1),
                WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE)) {
            return std::unexpected(Win32Message("WinHttpAddRequestHeaders"));
        }
        return {};
    };

    if (!authorization.empty()) {
        if (auto added = add_header(authorization); !added) {
            return std::unexpected(std::move(added.error()));
        }
    }
    if (!content_type.empty()) {
        if (auto added = add_header(content_type); !added) {
            return std::unexpected(std::move(added.error()));
        }
    }
    if (auto added = add_header(L"Accept: application/json"); !added) {
        return std::unexpected(std::move(added.error()));
    }

    const DWORD body_length = static_cast<DWORD>(body.size());
    LPVOID body_ptr = body.empty() ? WINHTTP_NO_REQUEST_DATA : const_cast<char*>(body.data());
    if (!WinHttpSendRequest(
            request.value,
            WINHTTP_NO_ADDITIONAL_HEADERS,
            0,
            body_ptr,
            body_length,
            body_length,
            0)) {
        return std::unexpected(Win32Message("WinHttpSendRequest"));
    }
    if (!WinHttpReceiveResponse(request.value, nullptr)) {
        return std::unexpected(Win32Message("WinHttpReceiveResponse"));
    }

    DWORD status = 0;
    DWORD status_size = sizeof(status);
    if (!WinHttpQueryHeaders(
            request.value,
            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX,
            &status,
            &status_size,
            WINHTTP_NO_HEADER_INDEX)) {
        return std::unexpected(Win32Message("WinHttpQueryHeaders"));
    }

    auto payload = ReadBody(request.value);
    if (!payload) {
        return std::unexpected(std::move(payload.error()));
    }
    return HttpResponse{status, std::move(*payload)};
}

}  // namespace bridge::net
