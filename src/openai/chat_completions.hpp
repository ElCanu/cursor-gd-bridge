#pragma once

#include <expected>
#include <string>
#include <string_view>

// OpenAI chat-completions subset that GitHub Desktop sends for commit messages.

namespace bridge::openai {

struct ChatError {
    int status = 400;
    std::string message;
};

struct ChatRequest {
    std::string model;
    std::string prompt;
    bool stream = false;
};

[[nodiscard]] std::expected<ChatRequest, ChatError> ParseChatCompletions(std::string_view body);
[[nodiscard]] std::string CompletionJson(std::string_view model, std::string_view text);
[[nodiscard]] std::string CompletionSse(std::string_view model, std::string_view text);
[[nodiscard]] std::string ErrorJson(std::string_view message);
[[nodiscard]] std::string CleanCommitMessage(std::string text);

}  // namespace bridge::openai
