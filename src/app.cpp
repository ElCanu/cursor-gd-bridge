// Local OpenAI server for GitHub Desktop. Binds 127.0.0.1 only.
// httplib.h must be included before Windows.h so Winsock2 wins.

#include "httplib.h"

#include "app.hpp"

#include "config/defaults.hpp"
#include "cursor/agent_client.hpp"
#include "openai/chat_completions.hpp"
#include "openai/models.hpp"
#include "util/log.hpp"
#include "util/single_instance.hpp"

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>

#include <cctype>
#include <chrono>
#include <exception>
#include <format>
#include <string>
#include <string_view>

namespace bridge {
namespace {

void NotifyAlreadyRunning() {
#if BRIDGE_CONSOLE
    log::Error("cursor-gd-bridge is already running");
#else
    MessageBoxW(
        nullptr,
        L"cursor-gd-bridge is already running.",
        L"cursor-gd-bridge",
        MB_OK | MB_ICONINFORMATION);
#endif
}

void NotifyFatal(const wchar_t* message) {
#if !BRIDGE_CONSOLE
    MessageBoxW(nullptr, message, L"cursor-gd-bridge", MB_OK | MB_ICONERROR);
#else
    (void)message;
#endif
}

// GitHub Desktop sends the provider API key as "Authorization: Bearer <key>".
// That value is the Cursor key; it is forwarded and never logged.
[[nodiscard]] std::string ApiKeyFromRequest(const httplib::Request& req) {
    std::string header = req.get_header_value("Authorization");
    if (header.empty()) {
        header = req.get_header_value("api-key");
    }
    const auto begin = header.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) {
        return {};
    }
    const auto end = header.find_last_not_of(" \t\r\n");
    header = header.substr(begin, end - begin + 1);

    constexpr std::string_view kBearer = "bearer ";
    if (header.size() > kBearer.size()) {
        bool is_bearer = true;
        for (std::size_t i = 0; i < kBearer.size(); ++i) {
            if (std::tolower(static_cast<unsigned char>(header[i])) != kBearer[i]) {
                is_bearer = false;
                break;
            }
        }
        if (is_bearer) {
            header.erase(0, kBearer.size());
        }
    }
    const auto token_begin = header.find_first_not_of(" \t");
    if (token_begin == std::string::npos) {
        return {};
    }
    return header.substr(token_begin);
}

}  // namespace

int Run() {
    SingleInstance instance;
    switch (instance.TryAcquire()) {
    case SingleInstance::Status::AlreadyRunning:
        NotifyAlreadyRunning();
        return 0;
    case SingleInstance::Status::Failed:
        log::Error("could not create the single-instance mutex");
        NotifyFatal(L"Could not create the single-instance mutex.");
        return 1;
    case SingleInstance::Status::Acquired:
        break;
    }

    try {
        cursor::AgentClient agents;
        httplib::Server server;
        server.set_payload_max_length(defaults::kMaxRequestBytes);
        server.set_read_timeout(std::chrono::seconds(120));
        server.set_write_timeout(std::chrono::seconds(120));
        server.set_exception_handler([](const httplib::Request&, httplib::Response& res, std::exception_ptr ep) {
            std::string message = "internal error";
            try {
                if (ep) {
                    std::rethrow_exception(ep);
                }
            } catch (const std::exception& ex) {
                message = ex.what();
            }
            log::Error(message);
            res.status = 500;
            res.set_content(openai::ErrorJson(message), "application/json; charset=utf-8");
        });

        server.Get("/health", [](const httplib::Request&, httplib::Response& res) {
            res.set_content("ok", "text/plain; charset=utf-8");
        });
        server.Get("/v1/models", [](const httplib::Request&, httplib::Response& res) {
            res.set_content(openai::ModelsListJson(), "application/json; charset=utf-8");
        });
        server.Post("/v1/chat/completions", [&agents](const httplib::Request& req, httplib::Response& res) {
            auto parsed = openai::ParseChatCompletions(req.body);
            if (!parsed) {
                res.status = parsed.error().status;
                res.set_content(openai::ErrorJson(parsed.error().message), "application/json; charset=utf-8");
                log::Error(parsed.error().message);
                return;
            }

            const std::string api_key = ApiKeyFromRequest(req);
            if (api_key.empty()) {
                constexpr std::string_view kMissing =
                    "missing API key; set the Cursor key in the GitHub Desktop provider";
                log::Error(kMissing);
                res.status = 401;
                res.set_content(openai::ErrorJson(kMissing), "application/json; charset=utf-8");
                return;
            }

            log::Info(std::format(
                "chat completion model={} stream={} bytes={}",
                parsed->model,
                parsed->stream,
                req.body.size()));

            auto result = agents.Complete(parsed->prompt, parsed->model, api_key);
            if (!result) {
                log::Error(result.error().message);
                res.status = result.error().http_status;
                res.set_content(openai::ErrorJson(result.error().message), "application/json; charset=utf-8");
                return;
            }

            const auto text = openai::CleanCommitMessage(std::move(*result));
            if (text.empty()) {
                constexpr std::string_view kEmpty = "empty commit message from Cursor";
                log::Error(kEmpty);
                res.status = 502;
                res.set_content(openai::ErrorJson(kEmpty), "application/json; charset=utf-8");
                return;
            }

            log::Info(std::format("commit message: {}", text));
            // Desktop may set stream=true. Emit one SSE burst after the agent finishes.
            if (parsed->stream) {
                res.set_header("Cache-Control", "no-cache");
                res.set_content(openai::CompletionSse(parsed->model, text), "text/event-stream; charset=utf-8");
            } else {
                res.set_content(openai::CompletionJson(parsed->model, text), "application/json; charset=utf-8");
            }
        });

        const auto host = std::string(defaults::kListenHost);
        log::Info(std::format("listening on http://{}:{}/v1", host, defaults::kListenPort));
        if (!server.listen(host, defaults::kListenPort)) {
            log::Error(std::format("listen failed on {}:{}", host, defaults::kListenPort));
            NotifyFatal(L"Could not listen on 127.0.0.1:18789. The port may already be in use.");
            return 1;
        }
    } catch (const std::exception& ex) {
        log::Error(ex.what());
        NotifyFatal(L"The bridge failed to start. See %LOCALAPPDATA%\\cursor-gd-bridge\\bridge.log.");
        return 1;
    }
    return 0;
}

}  // namespace bridge
