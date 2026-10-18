#pragma once

#include <filesystem>
#include <fstream>
#include <string>

namespace aetherwall {

std::string utf8(const std::wstring& value);

class Logger {
  public:
    explicit Logger(const std::filesystem::path& directory);
    void write(const char* level, const std::string& message);
    void info(const std::string& message) { write("INFO", message); }
    void warning(const std::string& message) { write("WARN", message); }
    void error(const std::string& message) { write("ERROR", message); }
    const std::filesystem::path& path() const { return path_; }

  private:
    std::filesystem::path path_;
    std::ofstream stream_;
    std::size_t bytesWritten_ = 0;
    bool capped_ = false;
};

} // namespace aetherwall
