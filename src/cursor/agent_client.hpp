#pragma once

#include <expected>
#include <memory>
#include <string>
#include <string_view>

// One-shot Cursor Cloud Agent. The diff is in the prompt; no repository is attached.

namespace bridge::cursor {

struct ClientError {
    int http_status = 502;
    std::string message;
};

class AgentClient {
public:
    AgentClient();
    ~AgentClient();

    AgentClient(const AgentClient&) = delete;
    AgentClient& operator=(const AgentClient&) = delete;

    // api_key is the bearer token GitHub Desktop sent on this request. It is not stored.
    [[nodiscard]] std::expected<std::string, ClientError> Complete(
        std::string_view prompt,
        std::string_view model,
        std::string_view api_key);

private:
    struct State;
    std::unique_ptr<State> state_;
};

}  // namespace bridge::cursor
