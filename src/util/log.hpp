#pragma once

#include <string_view>

// Debug: Info and Error go to stderr. Release: Info is dropped; Error is appended to
// %LOCALAPPDATA%\cursor-gd-bridge\bridge.log. Do not log the API key or full diffs.

namespace bridge::log {

void Info(std::string_view message);
void Error(std::string_view message);

}  // namespace bridge::log
