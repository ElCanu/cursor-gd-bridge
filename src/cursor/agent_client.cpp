#include "cursor/agent_client.hpp"

#include "config/defaults.hpp"
#include "net/winhttp_client.hpp"
#include "util/log.hpp"
#include "util/utf8.hpp"

#include "json.hpp"

#include <algorithm>
#include <chrono>
#include <format>
#include <mutex>
#include <string>
#include <thread>
#include <utility>

namespace bridge::cursor {
namespace {

[[nodiscard]] std::string Snip(std::string_view text) {
    constexpr std::size_t kMax = 400;
    std::string out(text.substr(0, std::min(text.size(), kMax)));
    for (char& ch : out) {
        if (ch == '\r' || ch == '\n') {
            ch = ' ';
        }
    }
    if (text.size() > kMax) {
        out += "...";
    }
    return out;
}

[[nodiscard]] int MapStatus(unsigned status) {
    if (status == 400) {
        return 400;
    }
    if (status == 401 || status == 403) {
        return 401;
    }
    if (status == 429) {
        return 429;
    }
    return 502;
}

[[nodiscard]] bool IsTerminalFailure(std::string_view status) {
    return status == "ERROR" || status == "CANCELLED" || status == "EXPIRED";
}

}  // namespace

struct AgentClient::State {
    net::WinHttpClient http;
    std::mutex mutex;
};

AgentClient::AgentClient() : state_(std::make_unique<State>()) {}

AgentClient::~AgentClient() = default;

std::expected<std::string, ClientError> AgentClient::Complete(
    std::string_view prompt,
    std::string_view model,
    std::string_view api_key) {
    if (prompt.empty()) {
        return std::unexpected(ClientError{400, "prompt is empty"});
    }
    if (api_key.empty()) {
        return std::unexpected(ClientError{401, "missing Cursor API key"});
    }
    if (model.empty()) {
        return std::unexpected(ClientError{400, "model must be a non-empty string"});
    }
    const std::string model_id(model);
    const std::wstring authorization = L"Authorization: Bearer " + Utf8ToWide(api_key);

    // One Cursor call at a time so a burst of Desktop retries cannot stampede the API.
    std::lock_guard lock(state_->mutex);

    // Omit repos and env: Cursor starts a no-repo agent and answers from the prompt only.
    const nlohmann::json payload = {
        {"prompt", {{"text", std::string(prompt)}}},
        {"model", {{"id", model_id}}},
    };
    const std::string body = payload.dump();
    auto created = state_->http.Request(
        L"POST",
        L"/v1/agents",
        body,
        L"Content-Type: application/json",
        authorization);
    if (!created) {
        return std::unexpected(ClientError{502, created.error()});
    }
    if (created->status != 200 && created->status != 201) {
        return std::unexpected(ClientError{
            MapStatus(created->status),
            std::format("Cursor HTTP {}: {}", created->status, Snip(created->body)),
        });
    }

    nlohmann::json created_json;
    try {
        created_json = nlohmann::json::parse(created->body);
    } catch (const nlohmann::json::exception& ex) {
        return std::unexpected(ClientError{502, std::format("Cursor create response is not JSON: {}", ex.what())});
    }

    if (!created_json.contains("agent") || !created_json["agent"].contains("id") ||
        !created_json.contains("run") || !created_json["run"].contains("id")) {
        return std::unexpected(ClientError{502, "Cursor create response is missing agent or run id"});
    }

    const auto agent_id = created_json["agent"]["id"].get<std::string>();
    const auto run_id = created_json["run"]["id"].get<std::string>();
    log::Info(std::format("cursor agent={} run={} model={}", agent_id, run_id, model_id));

    const auto& initial_run = created_json["run"];
    if (initial_run.contains("status") && initial_run["status"] == "FINISHED" &&
        initial_run.contains("result") && initial_run["result"].is_string()) {
        return initial_run["result"].get<std::string>();
    }

    const std::wstring run_path = std::format(
        L"/v1/agents/{}/runs/{}",
        Utf8ToWide(agent_id),
        Utf8ToWide(run_id));

    const auto deadline = std::chrono::steady_clock::now() + defaults::kCursorTimeout;
    auto delay = defaults::kPollInitial;
    std::string previous_status;

    while (std::chrono::steady_clock::now() < deadline) {
        auto polled = state_->http.Request(L"GET", run_path, {}, {}, authorization);
        if (!polled) {
            return std::unexpected(ClientError{502, polled.error()});
        }
        if (polled->status != 200) {
            return std::unexpected(ClientError{
                MapStatus(polled->status),
                std::format("Cursor run HTTP {}: {}", polled->status, Snip(polled->body)),
            });
        }

        nlohmann::json run_json;
        try {
            run_json = nlohmann::json::parse(polled->body);
        } catch (const nlohmann::json::exception& ex) {
            return std::unexpected(ClientError{502, std::format("Cursor run response is not JSON: {}", ex.what())});
        }
        if (!run_json.contains("status") || !run_json["status"].is_string()) {
            return std::unexpected(ClientError{502, "Cursor run response is missing status"});
        }

        const auto status = run_json["status"].get<std::string>();
        if (status != previous_status) {
            log::Info(std::format("cursor run {} status {}", run_id, status));
            previous_status = status;
        }

        if (status == "FINISHED") {
            if (run_json.contains("result") && run_json["result"].is_string()) {
                return run_json["result"].get<std::string>();
            }
            return std::unexpected(ClientError{502, "Cursor run finished without a result"});
        }
        if (IsTerminalFailure(status)) {
            std::string detail = status;
            if (run_json.contains("result") && run_json["result"].is_string()) {
                detail += ": ";
                detail += Snip(run_json["result"].get<std::string>());
            }
            return std::unexpected(ClientError{502, "Cursor run " + detail});
        }

        const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
            deadline - std::chrono::steady_clock::now());
        if (remaining <= std::chrono::milliseconds::zero()) {
            break;
        }
        std::this_thread::sleep_for(std::min(delay, remaining));
        delay = std::min(delay * 2, defaults::kPollMax);
    }

    return std::unexpected(ClientError{502, "Cursor agent timed out"});
}

}  // namespace bridge::cursor
