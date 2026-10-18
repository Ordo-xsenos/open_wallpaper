#include "ipc/NamedPipeServer.h"
#include "ipc/Protocol.h"
#include <array>
#include <chrono>
#include <sddl.h>

namespace aetherwall {
std::wstring currentLogonSid() {
    HANDLE raw = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &raw))
        throwLastError("OpenProcessToken");
    UniqueHandle token(raw);
    DWORD size = 0;
    GetTokenInformation(token.get(), TokenLogonSid, nullptr, 0, &size);
    std::vector<BYTE> bytes(size);
    if (!size || !GetTokenInformation(token.get(), TokenLogonSid, bytes.data(), size, &size))
        throwLastError("GetTokenInformation(TokenLogonSid)");
    const auto* groups = reinterpret_cast<const TOKEN_GROUPS*>(bytes.data());
    if (groups->GroupCount != 1)
        throw std::runtime_error("An interactive logon SID is required");
    LPWSTR text = nullptr;
    if (!ConvertSidToStringSidW(groups->Groups[0].Sid, &text))
        throwLastError("ConvertSidToStringSidW");
    std::wstring result(text);
    LocalFree(text);
    return result;
}
NamedPipeServer::NamedPipeServer(HWND control)
    : control_(control), stop_(CreateEventW(nullptr, TRUE, FALSE, nullptr)) {
    if (!stop_)
        throwLastError("CreateEventW(pipe stop)");
    const auto sddl = L"D:P(A;;GA;;;" + currentLogonSid() + L")";
    PSECURITY_DESCRIPTOR descriptor = nullptr;
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl.c_str(), SDDL_REVISION_1,
                                                              &descriptor, nullptr))
        throwLastError("Pipe security descriptor");
    SECURITY_ATTRIBUTES attributes{sizeof(SECURITY_ATTRIBUTES), descriptor, FALSE};
    pipe_.reset(CreateNamedPipeW(
        corePipeName, PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED | FILE_FLAG_FIRST_PIPE_INSTANCE,
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS, 1,
        static_cast<DWORD>(ipcMessageLimit + 4), static_cast<DWORD>(ipcMessageLimit + 4), 2000,
        &attributes));
    LocalFree(descriptor);
    if (!pipe_)
        throwLastError("CreateNamedPipeW");
    worker_ = std::thread([this] { run(); });
}
NamedPipeServer::~NamedPipeServer() {
    SetEvent(stop_.get());
    CancelIoEx(pipe_.get(), nullptr);
    {
        std::lock_guard lock(mutex_);
        if (pending_) {
            std::lock_guard jobLock(pending_->mutex);
            pending_->cancelled = true;
            pending_->changed.notify_all();
        }
    }
    if (worker_.joinable())
        worker_.join();
}
bool NamedPipeServer::transfer(HANDLE pipe, bool write, void* bytes, DWORD size,
                               ULONGLONG deadline) {
    auto* cursor = static_cast<BYTE*>(bytes);
    while (size) {
        UniqueHandle event(CreateEventW(nullptr, TRUE, FALSE, nullptr));
        if (!event)
            return false;
        OVERLAPPED overlapped{};
        overlapped.hEvent = event.get();
        DWORD count = 0;
        BOOL result = write ? WriteFile(pipe, cursor, size, &count, &overlapped)
                            : ReadFile(pipe, cursor, size, &count, &overlapped);
        if (!result && GetLastError() == ERROR_IO_PENDING) {
            HANDLE handles[]{stop_.get(), event.get()};
            const auto now = GetTickCount64();
            const DWORD waited = WaitForMultipleObjects(
                2, handles, FALSE, now < deadline ? static_cast<DWORD>(deadline - now) : 0);
            if (waited != WAIT_OBJECT_0 + 1) {
                CancelIoEx(pipe, &overlapped);
                GetOverlappedResult(pipe, &overlapped, &count, TRUE);
                return false;
            }
            result = GetOverlappedResult(pipe, &overlapped, &count, FALSE);
        }
        if (!result || !count)
            return false;
        cursor += count;
        size -= count;
    }
    return true;
}
void NamedPipeServer::run() noexcept {
    try {
        while (WaitForSingleObject(stop_.get(), 0) != WAIT_OBJECT_0) {
            UniqueHandle event(CreateEventW(nullptr, TRUE, FALSE, nullptr));
            if (!event)
                return;
            OVERLAPPED connect{};
            connect.hEvent = event.get();
            const BOOL connected = ConnectNamedPipe(pipe_.get(), &connect);
            const DWORD error = connected ? ERROR_SUCCESS : GetLastError();
            if (error == ERROR_IO_PENDING) {
                HANDLE handles[]{stop_.get(), event.get()};
                if (WaitForMultipleObjects(2, handles, FALSE, INFINITE) != WAIT_OBJECT_0 + 1) {
                    CancelIoEx(pipe_.get(), &connect);
                    DWORD ignored = 0;
                    GetOverlappedResult(pipe_.get(), &connect, &ignored, TRUE);
                    return;
                }
                DWORD ignored = 0;
                if (!GetOverlappedResult(pipe_.get(), &connect, &ignored, FALSE)) {
                    DisconnectNamedPipe(pipe_.get());
                    continue;
                }
            } else if (error != ERROR_SUCCESS && error != ERROR_PIPE_CONNECTED)
                return;
            const auto deadline = GetTickCount64() + 2000;
            try {
                std::array<std::uint8_t, 4> header{};
                if (!transfer(pipe_.get(), false, header.data(), 4, deadline))
                    throw std::runtime_error("Read timeout");
                std::string request(frameLength(header), '\0');
                if (!transfer(pipe_.get(), false, request.data(),
                              static_cast<DWORD>(request.size()), deadline))
                    throw std::runtime_error("Read timeout");
                auto job = std::make_shared<Job>();
                job->request = std::move(request);
                {
                    std::lock_guard lock(mutex_);
                    pending_ = job;
                }
                if (!PostMessageW(control_, readyMessage, 0, 0))
                    throw std::runtime_error("Core control window unavailable");
                std::unique_lock lock(job->mutex);
                job->changed.wait_for(lock, std::chrono::seconds(10),
                                      [&] { return job->done || job->cancelled; });
                if (!job->done) {
                    job->cancelled = true;
                    throw std::runtime_error("Command timeout");
                }
                auto response = frameMessage(job->response);
                lock.unlock();
                transfer(pipe_.get(), true, response.data(), static_cast<DWORD>(response.size()),
                         GetTickCount64() + 2000);
            } catch (
                ...) { /* Invalid or stalled peers are disconnected without executing commands. */
            }
            {
                std::lock_guard lock(mutex_);
                pending_.reset();
            }
            DisconnectNamedPipe(pipe_.get());
        }
    } catch (...) { /* Destruction still cancels I/O and joins the worker. */
    }
}
void NamedPipeServer::process(const std::function<std::string(std::string_view)>& handler) {
    std::shared_ptr<Job> job;
    {
        std::lock_guard lock(mutex_);
        job = pending_;
    }
    if (!job)
        return;
    std::unique_lock lock(job->mutex);
    if (job->cancelled || job->done)
        return;
    // Holding the job lock prevents its worker from abandoning an executing mutation.
    job->response = handler(job->request);
    job->done = true;
    lock.unlock();
    job->changed.notify_all();
}
} // namespace aetherwall
