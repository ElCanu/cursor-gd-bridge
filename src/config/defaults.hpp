#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace bridge::defaults {

// Loopback port GitHub Desktop should use as the OpenAI base URL (.../v1).
inline constexpr std::uint16_t kListenPort = 18789;
inline constexpr std::string_view kListenHost = "127.0.0.1";
inline constexpr std::wstring_view kCursorHost = L"api.cursor.com";

// Longer than GitHub Desktop's default 60s. Raise the Desktop timeout to match.
inline constexpr std::chrono::seconds kCursorTimeout{90};
inline constexpr std::chrono::milliseconds kPollInitial{250};
inline constexpr std::chrono::milliseconds kPollMax{1000};

inline constexpr int kHttpResolveMs = 10'000;
inline constexpr int kHttpConnectMs = 15'000;
inline constexpr int kHttpSendMs = 30'000;
inline constexpr int kHttpReceiveMs = 30'000;

inline constexpr std::size_t kMaxRequestBytes = 8 * 1024 * 1024;

// Desktop parses the assistant text as JSON: {"title","description"}.
// The diff and any repo style notes are appended after this prefix.
inline constexpr std::string_view kCommitInstruction =
    "GitHub Desktop expects your entire reply to be one JSON object.\n"
    "Rules:\n"
    "- Return only {\"title\":\"...\",\"description\":\"...\"}.\n"
    "- title and description are strings. description may be empty.\n"
    "- title is one imperative subject line, preferably 72 characters or fewer.\n"
    "- No markdown fences, no preamble, no explanation.\n"
    "- Do not use tools. Do not modify files. Answer from the conversation only.\n"
    "- If the conversation includes a repository commit style, follow it.\n";

}  // namespace bridge::defaults
