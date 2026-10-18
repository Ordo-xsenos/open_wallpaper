#include "diagnostics/Logger.h"
#include "system/WindowsSupport.h"

#include <ShlObj.h>

#include <algorithm>
#include <iostream>
#include <limits>
#include <memory>
#include <vector>

namespace aetherwall {
namespace {
std::string timestamp(bool filename) {
    SYSTEMTIME time{};
    GetSystemTime(&time);
    std::ostringstream value;
    value << std::setfill('0') << std::setw(4) << time.wYear << (filename ? "" : "-")
          << std::setw(2) << time.wMonth << (filename ? "" : "-") << std::setw(2) << time.wDay
          << (filename ? "_" : "T") << std::setw(2) << time.wHour << (filename ? "" : ":")
          << std::setw(2) << time.wMinute << (filename ? "" : ":") << std::setw(2) << time.wSecond
          << (filename ? "_" : ".") << std::setw(3) << time.wMilliseconds << (filename ? "" : "Z");
    return value.str();
}

std::filesystem::path defaultDirectory() {
    PWSTR raw = nullptr;
    checkHr(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_CREATE, nullptr, &raw),
            "SHGetKnownFolderPath");
    const std::unique_ptr<wchar_t, decltype(&CoTaskMemFree)> owner(raw, &CoTaskMemFree);
    return std::filesystem::path(raw) / L"AetherWall" / L"logs";
}

void pruneLogs(const std::filesystem::path& directory) {
    std::vector<std::filesystem::path> files;
    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        const auto filename = entry.path().filename().wstring();
        if (entry.is_regular_file() && filename.starts_with(L"m0_") &&
            entry.path().extension() == L".log") {
            files.push_back(entry.path());
        }
    }
    // UTC-prefixed filenames sort chronologically. Retain nine before opening the tenth.
    std::sort(files.begin(), files.end());
    for (std::size_t index = 0; index + 9 < files.size(); ++index) {
        std::error_code ignored;
        std::filesystem::remove(files[index], ignored);
    }
}
} // namespace

std::string utf8(const std::wstring& value) {
    if (value.empty()) {
        return {};
    }
    if (value.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        throw std::length_error("UTF-16 input too large");
    }
    const int length = static_cast<int>(value.size());
    const int size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), length,
                                         nullptr, 0, nullptr, nullptr);
    if (!size) {
        throwLastError("WideCharToMultiByte");
    }
    std::string result(static_cast<std::size_t>(size), '\0');
    if (!WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), length, result.data(),
                             size, nullptr, nullptr)) {
        throwLastError("WideCharToMultiByte");
    }
    return result;
}

Logger::Logger(const std::filesystem::path& directory) {
    const auto root = directory.empty() ? defaultDirectory() : directory;
    std::filesystem::create_directories(root);
    pruneLogs(root);
    path_ = root / ("m0_" + timestamp(true) + "_" + std::to_string(GetCurrentProcessId()) + ".log");
    stream_.open(path_, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!stream_) {
        throw std::runtime_error("Cannot open diagnostic log");
    }
    info("log=" + utf8(std::filesystem::absolute(path_).wstring()));
}

void Logger::write(const char* level, const std::string& message) {
    constexpr std::size_t byteLimit = 4 * 1024 * 1024;
    if (capped_) {
        return;
    }
    std::string escaped = message;
    std::replace(escaped.begin(), escaped.end(), '\n', ' ');
    std::replace(escaped.begin(), escaped.end(), '\r', ' ');
    std::string line = timestamp(false) + " [" + level + "] " + escaped + "\n";
    if (bytesWritten_ + line.size() > byteLimit) {
        line = timestamp(false) + " [WARN] Log size limit reached; further entries suppressed.\n";
        capped_ = true;
    }
    stream_ << line;
    stream_.flush();
    bytesWritten_ += line.size();
    OutputDebugStringA(line.c_str());
    std::cerr << line;
}

} // namespace aetherwall
