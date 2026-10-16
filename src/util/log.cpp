#include "util/log.hpp"

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>
#include <shlobj.h>

#include <chrono>
#include <ctime>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <mutex>
#include <string>

namespace bridge::log {
namespace {

std::mutex g_log_mutex;

[[nodiscard]] std::string Timestamp() {
    const auto now = std::chrono::system_clock::now();
    const auto time = std::chrono::system_clock::to_time_t(now);
    std::tm local{};
    localtime_s(&local, &time);
    return std::format(
        "{:04}-{:02}-{:02} {:02}:{:02}:{:02}",
        local.tm_year + 1900,
        local.tm_mon + 1,
        local.tm_mday,
        local.tm_hour,
        local.tm_min,
        local.tm_sec);
}

void AppendErrorFile(std::string_view line) {
    PWSTR raw = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &raw)) || raw == nullptr) {
        return;
    }
    std::filesystem::path path(raw);
    CoTaskMemFree(raw);
    path /= "cursor-gd-bridge";
    std::error_code ec;
    std::filesystem::create_directories(path, ec);
    if (ec) {
        return;
    }
    path /= "bridge.log";
    std::ofstream out(path, std::ios::app);
    if (!out) {
        return;
    }
    out << line << '\n';
}

}  // namespace

// Console builds only. Release drops the message. Callers still format the argument.
void Info(std::string_view message) {
#if BRIDGE_CONSOLE
    std::lock_guard lock(g_log_mutex);
    std::cerr << std::format("[{}] INFO {}\n", Timestamp(), message);
#else
    (void)message;
#endif
}

void Error(std::string_view message) {
    std::lock_guard lock(g_log_mutex);
    const auto line = std::format("[{}] ERROR {}", Timestamp(), message);
#if BRIDGE_CONSOLE
    std::cerr << line << '\n';
#endif
    AppendErrorFile(line);
}

}  // namespace bridge::log
