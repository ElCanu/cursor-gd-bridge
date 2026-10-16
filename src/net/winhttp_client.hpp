#pragma once

#include <expected>
#include <string>
#include <string_view>
#include <utility>

// Outbound HTTPS client for api.cursor.com. One WinHTTP session is reused for keep-alive.
// Request() is not safe to call from two threads at once; AgentClient serializes it.

namespace bridge::net {

struct HttpResponse {
    unsigned status = 0;
    std::string body;
};

class WinHttpClient {
public:
    WinHttpClient();
    ~WinHttpClient();

    WinHttpClient(const WinHttpClient&) = delete;
    WinHttpClient& operator=(const WinHttpClient&) = delete;

    [[nodiscard]] std::expected<HttpResponse, std::string> Request(
        std::wstring_view method,
        std::wstring_view path,
        std::string_view body,
        std::wstring_view content_type,
        std::wstring_view authorization);

private:
    struct Handles;
    Handles* handles_ = nullptr;
};

}  // namespace bridge::net
