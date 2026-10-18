#include "ipc/NamedPipeServer.h"
#include "ipc/Protocol.h"
#include <array>
#include <sddl.h>

namespace aetherwall {
std::string pipeExchange(std::string_view request) {
    const auto deadline = GetTickCount64() + 12000;
    if (!WaitNamedPipeW(corePipeName, 1000))
        throw std::runtime_error("Core is not available");
    UniqueHandle pipe(CreateFileW(
        corePipeName, GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
        FILE_FLAG_OVERLAPPED | SECURITY_SQOS_PRESENT | SECURITY_IDENTIFICATION, nullptr));
    if (!pipe)
        throwLastError("Connect to Core");
    ULONG pid = 0;
    if (!GetNamedPipeServerProcessId(pipe.get(), &pid))
        throwLastError("GetNamedPipeServerProcessId");
    DWORD ownSession = 0, peerSession = 0;
    if (!ProcessIdToSessionId(GetCurrentProcessId(), &ownSession) ||
        !ProcessIdToSessionId(pid, &peerSession) || ownSession != peerSession)
        throw std::runtime_error("Core belongs to another session");
    UniqueHandle process(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid));
    HANDLE raw = nullptr;
    if (!process || !OpenProcessToken(process.get(), TOKEN_QUERY, &raw))
        throw std::runtime_error("Cannot verify Core identity");
    UniqueHandle token(raw);
    DWORD length = 0;
    GetTokenInformation(token.get(), TokenLogonSid, nullptr, 0, &length);
    std::vector<BYTE> bytes(length);
    if (!length || !GetTokenInformation(token.get(), TokenLogonSid, bytes.data(), length, &length))
        throw std::runtime_error("Cannot verify Core logon");
    const auto* groups = reinterpret_cast<const TOKEN_GROUPS*>(bytes.data());
    LPWSTR sid = nullptr;
    if (groups->GroupCount != 1 || !ConvertSidToStringSidW(groups->Groups[0].Sid, &sid))
        throw std::runtime_error("Invalid Core identity");
    const std::wstring peerSid(sid);
    LocalFree(sid);
    if (peerSid != currentLogonSid())
        throw std::runtime_error("Core belongs to another logon");
    auto transfer = [&](bool write, void* data, DWORD size) {
        auto* cursor = static_cast<BYTE*>(data);
        while (size) {
            UniqueHandle event(CreateEventW(nullptr, TRUE, FALSE, nullptr));
            if (!event)
                throwLastError("CreateEvent(client)");
            OVERLAPPED operation{};
            operation.hEvent = event.get();
            DWORD count = 0;
            BOOL okay = write ? WriteFile(pipe.get(), cursor, size, &count, &operation)
                              : ReadFile(pipe.get(), cursor, size, &count, &operation);
            if (!okay && GetLastError() == ERROR_IO_PENDING) {
                const auto now = GetTickCount64();
                if (WaitForSingleObject(event.get(),
                                        now < deadline ? static_cast<DWORD>(deadline - now) : 0) !=
                    WAIT_OBJECT_0) {
                    CancelIoEx(pipe.get(), &operation);
                    GetOverlappedResult(pipe.get(), &operation, &count, TRUE);
                    throw std::runtime_error(
                        "Core response timed out; refresh state before retrying");
                }
                okay = GetOverlappedResult(pipe.get(), &operation, &count, FALSE);
            }
            if (!okay || !count)
                throw std::runtime_error("Core disconnected");
            cursor += count;
            size -= count;
        }
    };
    auto frame = frameMessage(request);
    transfer(true, frame.data(), static_cast<DWORD>(frame.size()));
    std::array<std::uint8_t, 4> header{};
    transfer(false, header.data(), 4);
    std::string response(frameLength(header), '\0');
    transfer(false, response.data(), static_cast<DWORD>(response.size()));
    return response;
}
} // namespace aetherwall
