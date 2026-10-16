#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>

#include <stdexcept>
#include <string>
#include <string_view>

// WinHTTP paths and headers are UTF-16. Request bodies stay UTF-8.

namespace bridge {

[[nodiscard]] inline std::wstring Utf8ToWide(std::string_view text) {
    if (text.empty()) {
        return {};
    }
    if (text.size() > static_cast<std::size_t>(INT_MAX)) {
        throw std::runtime_error("UTF-8 string is too large");
    }
    const int needed = MultiByteToWideChar(
        CP_UTF8,
        0,
        text.data(),
        static_cast<int>(text.size()),
        nullptr,
        0);
    if (needed <= 0) {
        throw std::runtime_error("UTF-8 conversion failed");
    }
    std::wstring out(static_cast<std::size_t>(needed), L'\0');
    const int written = MultiByteToWideChar(
        CP_UTF8,
        0,
        text.data(),
        static_cast<int>(text.size()),
        out.data(),
        needed);
    if (written != needed) {
        throw std::runtime_error("UTF-8 conversion failed");
    }
    return out;
}

}  // namespace bridge
