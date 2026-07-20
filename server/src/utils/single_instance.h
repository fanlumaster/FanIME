#pragma once

#include <windows.h>

namespace CommonUtils
{
class SingleInstanceGuard
{
public:
    explicit SingleInstanceGuard(const wchar_t *mutex_name) noexcept
        : handle_(CreateMutexW(nullptr, FALSE, mutex_name)), creation_error_(GetLastError())
    {
    }

    ~SingleInstanceGuard()
    {
        if (handle_) CloseHandle(handle_);
    }

    SingleInstanceGuard(const SingleInstanceGuard &) = delete;
    SingleInstanceGuard &operator=(const SingleInstanceGuard &) = delete;

    [[nodiscard]] bool is_valid() const noexcept { return handle_ != nullptr; }
    [[nodiscard]] bool already_running() const noexcept
    {
        return is_valid() && creation_error_ == ERROR_ALREADY_EXISTS;
    }

private:
    HANDLE handle_ = nullptr;
    DWORD creation_error_ = ERROR_SUCCESS;
};
} // namespace CommonUtils
