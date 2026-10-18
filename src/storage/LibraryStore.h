#pragma once
#include "ipc/Protocol.h"
#include "scenes/Scene.h"
#include <filesystem>
#include <sqlite3.h>

namespace aetherwall {
std::string pathText(const std::filesystem::path& path);
std::filesystem::path textPath(const std::string& path);
struct DownloadReceipt {
    std::string sceneId, sha256, source;
    std::uintmax_t size = 0;
};
class LibraryStore {
  public:
    explicit LibraryStore(const std::filesystem::path& directory);
    ~LibraryStore();
    LibraryStore(const LibraryStore&) = delete;
    LibraryStore& operator=(const LibraryStore&) = delete;
    Json list() const;
    std::string importScene(const std::filesystem::path& source,
                            const DownloadReceipt* receipt = nullptr);
    Json downloadHistory() const;
    std::filesystem::path scenePath(const std::string& id) const;
    void favorite(const std::string& id, bool enabled);
    void remove(const std::string& id);
    void assign(const std::vector<std::string>& monitors, const std::string& id);
    Json assignments() const;
    const std::filesystem::path& directory() const { return directory_; }

  private:
    void execute(const char* sql) const;
    std::filesystem::path directory_;
    sqlite3* database_ = nullptr;
};
} // namespace aetherwall
