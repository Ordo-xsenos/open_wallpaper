#include "storage/SettingsStore.h"
#include <fstream>
#include <random>
#include <stdexcept>
#ifdef _WIN32
#include <Windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace aetherwall {
void atomicWrite(const std::filesystem::path& path, std::string_view bytes) {
    std::filesystem::create_directories(path.parent_path());
    const auto suffix = ".tmp-" + std::to_string(std::random_device{}());
    const auto temp = std::filesystem::path(path.native() + std::filesystem::path(suffix).native());
    const auto backup =
        std::filesystem::path(path.native() + std::filesystem::path(".bak").native());
    try {
#ifdef _WIN32
        HANDLE file = CreateFileW(temp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                                  FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE)
            throw std::runtime_error("Cannot create temporary settings file");
        DWORD written = 0;
        const bool saved =
            WriteFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr) &&
            written == bytes.size() && FlushFileBuffers(file);
        CloseHandle(file);
        if (!saved)
            throw std::runtime_error("Cannot flush settings");
        if (std::filesystem::exists(path)) {
            if (!ReplaceFileW(path.c_str(), temp.c_str(), backup.c_str(), 0, nullptr, nullptr))
                throw std::runtime_error("Cannot atomically replace settings");
        } else if (!MoveFileExW(temp.c_str(), path.c_str(), MOVEFILE_WRITE_THROUGH))
            throw std::runtime_error("Cannot commit initial settings");
#else
        const int file = open(temp.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
        if (file < 0)
            throw std::runtime_error("Cannot create temporary settings file");
        std::size_t done = 0;
        while (done < bytes.size()) {
            const auto count = write(file, bytes.data() + done, bytes.size() - done);
            if (count <= 0) {
                close(file);
                throw std::runtime_error("Cannot write settings");
            }
            done += static_cast<std::size_t>(count);
        }
        const bool flushed = fsync(file) == 0;
        close(file);
        if (!flushed)
            throw std::runtime_error("Cannot flush settings");
        if (std::filesystem::exists(path))
            std::filesystem::copy_file(path, backup,
                                       std::filesystem::copy_options::overwrite_existing);
        std::filesystem::rename(temp, path);
        const int directory = open(path.parent_path().c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
        if (directory >= 0) {
            fsync(directory);
            close(directory);
        }
#endif
    } catch (...) {
        std::error_code ignored;
        std::filesystem::remove(temp, ignored);
        throw;
    }
}
} // namespace aetherwall
