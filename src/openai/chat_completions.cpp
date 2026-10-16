#include "openai/chat_completions.hpp"

#include "config/defaults.hpp"

#include "json.hpp"

#include <chrono>
#include <string>
#include <utility>

namespace bridge::openai {
namespace {

[[nodiscard]] std::string ContentToString(const nlohmann::json& content) {
    if (content.is_string()) {
        return content.get<std::string>();
    }
    if (content.is_array()) {
        std::string out;
        for (const auto& part : content) {
            if (part.is_string()) {
                if (!out.empty()) {
                    out.push_back('\n');
                }
                out += part.get<std::string>();
            } else if (part.is_object()) {
                const nlohmann::json* text = nullptr;
                if (part.contains("text") && part["text"].is_string()) {
                    text = &part["text"];
                } else if (part.contains("content") && part["content"].is_string()) {
                    text = &part["content"];
                }
                if (text != nullptr) {
                    if (!out.empty()) {
                        out.push_back('\n');
                    }
                    out += text->get<std::string>();
                }
            }
        }
        return out;
    }
    if (content.is_null()) {
        return {};
    }
    return content.dump();
}

[[nodiscard]] std::int64_t UnixNow() {
    return static_cast<std::int64_t>(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
}

[[nodiscard]] std::string CompletionId() {
    return "chatcmpl-bridge-" + std::to_string(UnixNow());
}

void Trim(std::string& text) {
    const auto begin = text.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) {
        text.clear();
        return;
    }
    const auto end = text.find_last_not_of(" \t\r\n");
    text = text.substr(begin, end - begin + 1);
}

}  // namespace

std::expected<ChatRequest, ChatError> ParseChatCompletions(std::string_view body) {
    nlohmann::json doc;
    try {
        doc = nlohmann::json::parse(body);
    } catch (const nlohmann::json::exception& ex) {
        return std::unexpected(ChatError{400, std::string("invalid JSON: ") + ex.what()});
    }
    if (!doc.is_object()) {
        return std::unexpected(ChatError{400, "JSON body must be an object"});
    }
    if (!doc.contains("messages") || !doc["messages"].is_array() || doc["messages"].empty()) {
        return std::unexpected(ChatError{400, "messages must be a non-empty array"});
    }

    // Forward the id GitHub Desktop put on the selected model. Do not rewrite it.
    if (!doc.contains("model") || !doc["model"].is_string()) {
        return std::unexpected(ChatError{400, "model must be a string"});
    }
    std::string model = doc["model"].get<std::string>();
    if (model.empty()) {
        return std::unexpected(ChatError{400, "model must be a non-empty string"});
    }

    bool stream = false;
    if (doc.contains("stream") && !doc["stream"].is_null()) {
        if (!doc["stream"].is_boolean()) {
            return std::unexpected(ChatError{400, "stream must be a boolean"});
        }
        stream = doc["stream"].get<bool>();
    }

    std::string prompt;
    prompt.reserve(body.size() + defaults::kCommitInstruction.size() + 64);
    prompt.append(defaults::kCommitInstruction);
    prompt.append("\n\n");
    for (const auto& message : doc["messages"]) {
        if (!message.is_object()) {
            return std::unexpected(ChatError{400, "each message must be an object"});
        }
        std::string role = "user";
        if (message.contains("role") && message["role"].is_string()) {
            role = message["role"].get<std::string>();
        }
        std::string content;
        if (message.contains("content")) {
            content = ContentToString(message["content"]);
        }
        prompt.append("### ");
        prompt.append(role);
        prompt.push_back('\n');
        prompt.append(content);
        prompt.append("\n\n");
    }

    if (prompt.size() < defaults::kCommitInstruction.size() + 8) {
        return std::unexpected(ChatError{400, "messages did not contain any text"});
    }
    return ChatRequest{std::move(model), std::move(prompt), stream};
}

std::string CompletionJson(std::string_view model, std::string_view text) {
    const nlohmann::json doc = {
        {"id", CompletionId()},
        {"object", "chat.completion"},
        {"created", UnixNow()},
        {"model", model},
        {"choices",
         nlohmann::json::array(
             {{{"index", 0},
               {"message", {{"role", "assistant"}, {"content", text}}},
               {"finish_reason", "stop"}}})},
        {"usage", {{"prompt_tokens", 0}, {"completion_tokens", 0}, {"total_tokens", 0}}},
    };
    return doc.dump();
}

std::string CompletionSse(std::string_view model, std::string_view text) {
    const auto id = CompletionId();
    const auto created = UnixNow();
    const nlohmann::json first = {
        {"id", id},
        {"object", "chat.completion.chunk"},
        {"created", created},
        {"model", model},
        {"choices",
         nlohmann::json::array(
             {{{"index", 0},
               {"delta", {{"role", "assistant"}, {"content", text}}},
               {"finish_reason", nullptr}}})},
    };
    const nlohmann::json stop = {
        {"id", id},
        {"object", "chat.completion.chunk"},
        {"created", created},
        {"model", model},
        {"choices", nlohmann::json::array({{{"index", 0}, {"delta", nlohmann::json::object()}, {"finish_reason", "stop"}}})},
    };
    std::string sse;
    sse.reserve(first.dump().size() + stop.dump().size() + 64);
    sse.append("data: ");
    sse.append(first.dump());
    sse.append("\n\n");
    sse.append("data: ");
    sse.append(stop.dump());
    sse.append("\n\n");
    sse.append("data: [DONE]\n\n");
    return sse;
}

std::string ErrorJson(std::string_view message) {
    const nlohmann::json doc = {
        {"error", {{"message", message}, {"type", "bridge_error"}}},
    };
    return doc.dump();
}

[[nodiscard]] std::string PayloadFromObject(const nlohmann::json& doc) {
    if (!doc.is_object() || !doc.contains("title") || !doc["title"].is_string()) {
        return {};
    }
    const auto title = doc["title"].get<std::string>();
    if (title.empty()) {
        return {};
    }
    std::string description;
    if (doc.contains("description") && doc["description"].is_string()) {
        description = doc["description"].get<std::string>();
    }
    const nlohmann::json payload = {
        {"title", title},
        {"description", description},
    };
    return payload.dump();
}

[[nodiscard]] std::string FirstLine(std::string text) {
    Trim(text);
    const auto line_end = text.find_first_of("\r\n");
    if (line_end != std::string::npos) {
        text.erase(line_end);
    }
    Trim(text);
    if (text.size() >= 2) {
        const char open = text.front();
        const char close = text.back();
        if ((open == '"' && close == '"') || (open == '\'' && close == '\'')) {
            text = text.substr(1, text.size() - 2);
            Trim(text);
        }
    }
    return text;
}

// Desktop JSON.parses the assistant text and requires a title string.
// Agents often add fences or a one-line subject; both become {"title","description"}.
std::string CleanCommitMessage(std::string text) {
    Trim(text);
    if (text.starts_with("```")) {
        const auto first_nl = text.find('\n');
        if (first_nl != std::string::npos) {
            text.erase(0, first_nl + 1);
        }
        if (const auto fence = text.rfind("```"); fence != std::string::npos) {
            text.erase(fence);
        }
        Trim(text);
    }

    const auto object_begin = text.find('{');
    const auto object_end = text.rfind('}');
    if (object_begin != std::string::npos && object_end != std::string::npos && object_end > object_begin) {
        try {
            const auto parsed = nlohmann::json::parse(text.substr(object_begin, object_end - object_begin + 1));
            if (const auto payload = PayloadFromObject(parsed); !payload.empty()) {
                return payload;
            }
        } catch (const nlohmann::json::exception&) {
        }
    }

    const auto title = FirstLine(text);
    if (title.empty()) {
        return {};
    }
    const nlohmann::json payload = {
        {"title", title},
        {"description", ""},
    };
    return payload.dump();
}

}  // namespace bridge::openai
