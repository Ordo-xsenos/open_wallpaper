#pragma once
#include "system/WindowsSupport.h"
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>

namespace aetherwall {
class NamedPipeServer {
  public:
    static constexpr UINT readyMessage = WM_APP + 41;
    explicit NamedPipeServer(HWND control);
    ~NamedPipeServer();
    void process(const std::function<std::string(std::string_view)>& handler);

  private:
    struct Job {
        std::string request, response;
        std::mutex mutex;
        std::condition_variable changed;
        bool done = false, cancelled = false;
    };
    void run() noexcept;
    bool transfer(HANDLE pipe, bool write, void* bytes, DWORD size, ULONGLONG deadline);
    HWND control_;
    UniqueHandle stop_, pipe_;
    std::thread worker_;
    std::mutex mutex_;
    std::shared_ptr<Job> pending_;
};
constexpr wchar_t corePipeName[] = L"\\\\.\\pipe\\AetherWall-Core-v1";
std::wstring currentLogonSid();
std::string pipeExchange(std::string_view request);
} // namespace aetherwall
