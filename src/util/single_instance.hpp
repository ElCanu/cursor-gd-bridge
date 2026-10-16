#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>

namespace bridge {

// Named mutex so a second double-click exits instead of binding the same port.
// Global\ needs SeCreateGlobalPrivilege; a normal user falls back to Local\.
// WAIT_ABANDONED means the previous process crashed and the lock can be taken.
class SingleInstance {
public:
    enum class Status { Acquired, AlreadyRunning, Failed };

    SingleInstance() = default;
    ~SingleInstance() {
        if (owned_ && mutex_ != nullptr) {
            ReleaseMutex(mutex_);
        }
        if (mutex_ != nullptr) {
            CloseHandle(mutex_);
        }
    }

    SingleInstance(const SingleInstance&) = delete;
    SingleInstance& operator=(const SingleInstance&) = delete;

    [[nodiscard]] Status TryAcquire() {
        Status status = Status::Failed;
        if (TryName(L"Global\\cursor-gd-bridge", status)) {
            return status;
        }
        if (GetLastError() == ERROR_ACCESS_DENIED) {
            if (TryName(L"Local\\cursor-gd-bridge", status)) {
                return status;
            }
        }
        return Status::Failed;
    }

private:
    [[nodiscard]] bool TryName(const wchar_t* name, Status& status) {
        HANDLE mutex = CreateMutexW(nullptr, FALSE, name);
        if (mutex == nullptr) {
            return false;
        }
        const DWORD wait = WaitForSingleObject(mutex, 0);
        if (wait == WAIT_OBJECT_0 || wait == WAIT_ABANDONED) {
            mutex_ = mutex;
            owned_ = true;
            status = Status::Acquired;
            return true;
        }
        CloseHandle(mutex);
        if (wait == WAIT_TIMEOUT) {
            status = Status::AlreadyRunning;
            return true;
        }
        return false;
    }

    HANDLE mutex_ = nullptr;
    bool owned_ = false;
};

}  // namespace bridge
