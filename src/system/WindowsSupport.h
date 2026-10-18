#pragma once

#include <Windows.h>

#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

namespace aetherwall {

inline std::string hexCode(unsigned long code) {
    std::ostringstream text;
    text << "0x" << std::hex << std::setw(8) << std::setfill('0') << code;
    return text.str();
}

class WindowsError : public std::runtime_error {
  public:
    WindowsError(const std::string& operation, HRESULT result)
        : std::runtime_error(operation + " failed: " + hexCode(static_cast<unsigned long>(result))),
          result_(result) {}
    HRESULT result() const { return result_; }

  private:
    HRESULT result_;
};

inline void checkHr(HRESULT result, const char* operation) {
    if (FAILED(result)) {
        throw WindowsError(operation, result);
    }
}

inline void throwLastError(const char* operation) {
    const DWORD error = GetLastError();
    throw WindowsError(operation,
                       HRESULT_FROM_WIN32(error == ERROR_SUCCESS ? ERROR_GEN_FAILURE : error));
}

class UniqueHandle {
  public:
    UniqueHandle() = default;
    explicit UniqueHandle(HANDLE value) : value_(value) {}
    ~UniqueHandle() { reset(); }
    UniqueHandle(const UniqueHandle&) = delete;
    UniqueHandle& operator=(const UniqueHandle&) = delete;
    UniqueHandle(UniqueHandle&& other) noexcept : value_(std::exchange(other.value_, nullptr)) {}
    UniqueHandle& operator=(UniqueHandle&& other) noexcept {
        if (this != &other) {
            reset(std::exchange(other.value_, nullptr));
        }
        return *this;
    }
    HANDLE get() const { return value_; }
    explicit operator bool() const { return value_ && value_ != INVALID_HANDLE_VALUE; }
    void reset(HANDLE value = nullptr) {
        if (*this) {
            CloseHandle(value_);
        }
        value_ = value;
    }

  private:
    HANDLE value_ = nullptr;
};

class ScopedDpiContext {
  public:
    explicit ScopedDpiContext(DPI_AWARENESS_CONTEXT context)
        : previous_(SetThreadDpiAwarenessContext(context)) {
        if (!previous_) {
            throwLastError("SetThreadDpiAwarenessContext");
        }
    }
    ~ScopedDpiContext() { SetThreadDpiAwarenessContext(previous_); }
    ScopedDpiContext(const ScopedDpiContext&) = delete;
    ScopedDpiContext& operator=(const ScopedDpiContext&) = delete;

  private:
    DPI_AWARENESS_CONTEXT previous_;
};

} // namespace aetherwall
