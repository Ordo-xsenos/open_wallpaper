#include "storage/LibraryStore.h"
#include "scenes/ScenePackage.h"
#include "storage/PackageHash.h"
#include <chrono>
#include <fstream>
#include <random>
#include <stdexcept>

namespace aetherwall {
namespace {
class Statement {
  public:
    Statement(sqlite3* db, const char* sql) : database_(db) {
        if (sqlite3_prepare_v2(db, sql, -1, &value_, nullptr) != SQLITE_OK)
            throw std::runtime_error(sqlite3_errmsg(db));
    }
    ~Statement() { sqlite3_finalize(value_); }
    void bind(int index, const std::string& text) {
        if (sqlite3_bind_text(value_, index, text.c_str(), static_cast<int>(text.size()),
                              SQLITE_TRANSIENT) != SQLITE_OK)
            throw std::runtime_error(sqlite3_errmsg(database_));
    }
    bool step() {
        const int result = sqlite3_step(value_);
        if (result != SQLITE_ROW && result != SQLITE_DONE)
            throw std::runtime_error(sqlite3_errmsg(database_));
        return result == SQLITE_ROW;
    }
    std::string text(int index) const {
        const auto* value = sqlite3_column_text(value_, index);
        return value ? reinterpret_cast<const char*>(value) : "";
    }

  private:
    sqlite3* database_;
    sqlite3_stmt* value_ = nullptr;
};
void writeBytes(const std::filesystem::path& path, std::span<const std::uint8_t> bytes) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file.write(reinterpret_cast<const char*>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
    file.close();
    if (!file)
        throw std::runtime_error("Cannot write imported scene snapshot");
}
} // namespace
std::string pathText(const std::filesystem::path& path) {
    const auto bytes = path.u8string();
    return std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size());
}
std::filesystem::path textPath(const std::string& path) {
    return std::filesystem::path(
        std::u8string(reinterpret_cast<const char8_t*>(path.data()), path.size()));
}
LibraryStore::LibraryStore(const std::filesystem::path& directory)
    : directory_(std::filesystem::absolute(directory)) {
    std::filesystem::create_directories(directory_ / "library");
    const auto name = pathText(directory_ / "library.sqlite3");
    if (sqlite3_open_v2(name.c_str(), &database_, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
                        nullptr) != SQLITE_OK) {
        const std::string error = database_ ? sqlite3_errmsg(database_) : "SQLite open failed";
        sqlite3_close(database_);
        database_ = nullptr;
        throw std::runtime_error(error);
    }
    try {
        sqlite3_busy_timeout(database_, 1000);
        execute("PRAGMA foreign_keys=ON; PRAGMA journal_mode=WAL;");
        Statement version(database_, "PRAGMA user_version");
        version.step();
        if (std::stoi(version.text(0)) > 2)
            throw std::runtime_error("Library database schema is newer than this Core");
        execute("BEGIN; CREATE TABLE IF NOT EXISTS scenes(id TEXT PRIMARY KEY, title TEXT NOT "
                "NULL, author TEXT NOT NULL, license TEXT NOT NULL, preview TEXT NOT NULL, "
                "favorite INTEGER NOT NULL DEFAULT 0, installed INTEGER NOT NULL);"
                "CREATE TABLE IF NOT EXISTS assignments(monitor TEXT PRIMARY KEY, scene TEXT NOT "
                "NULL REFERENCES scenes(id) ON DELETE RESTRICT);"
                "CREATE TABLE IF NOT EXISTS history(id INTEGER PRIMARY KEY, scene TEXT NOT NULL, "
                "action TEXT NOT NULL, recorded INTEGER NOT NULL); CREATE TABLE IF NOT EXISTS "
                "downloads(id INTEGER PRIMARY KEY, scene TEXT NOT NULL, sha256 TEXT NOT NULL, "
                "source TEXT NOT NULL, bytes INTEGER NOT NULL, recorded INTEGER NOT NULL); PRAGMA "
                "user_version=2; COMMIT;");
    } catch (...) {
        sqlite3_close(database_);
        database_ = nullptr;
        throw;
    }
}
LibraryStore::~LibraryStore() { sqlite3_close(database_); }
void LibraryStore::execute(const char* sql) const {
    if (sqlite3_exec(database_, sql, nullptr, nullptr, nullptr) != SQLITE_OK)
        throw std::runtime_error(sqlite3_errmsg(database_));
}
Json LibraryStore::list() const {
    Statement query(database_, "SELECT id,title,author,license,preview,favorite FROM scenes ORDER "
                               "BY favorite DESC, title COLLATE NOCASE, id LIMIT 128");
    Json result = Json::array();
    while (query.step()) {
        const auto path = scenePath(query.text(0));
        result.push_back({{"id", query.text(0)},
                          {"title", query.text(1)},
                          {"author", query.text(2)},
                          {"license", query.text(3)},
                          {"previewPath", pathText(path / query.text(4))},
                          {"favorite", query.text(5) == "1"},
                          {"path", pathText(path)}});
    }
    return result;
}
std::filesystem::path LibraryStore::scenePath(const std::string& id) const {
    Statement query(database_, "SELECT id FROM scenes WHERE id=?");
    query.bind(1, id);
    if (!query.step())
        throw std::invalid_argument("Scene is not installed");
    // Defend paths even when an existing metadata database has been corrupted.
    const auto key = query.text(0);
    if (key.size() != 36)
        throw std::runtime_error("Invalid stored scene ID");
    for (std::size_t i = 0; i < key.size(); ++i) {
        const bool dash = i == 8 || i == 13 || i == 18 || i == 23;
        if (dash ? key[i] != '-'
                 : !((key[i] >= '0' && key[i] <= '9') || (key[i] >= 'a' && key[i] <= 'f')))
            throw std::runtime_error("Invalid stored scene ID");
    }
    return directory_ / "library" / query.text(0);
}
std::string LibraryStore::importScene(const std::filesystem::path& source,
                                      const DownloadReceipt* receipt) {
    struct TemporarySnapshot {
        std::filesystem::path directory;
        ~TemporarySnapshot() {
            if (!directory.empty()) {
                std::error_code ignored;
                std::filesystem::remove_all(directory, ignored);
            }
        }
    } snapshot;
    auto input = source;
    if (receipt) {
        if (!receipt->size || receipt->size > SceneLimits::packageBytes ||
            !std::filesystem::is_regular_file(source) ||
            std::filesystem::is_symlink(std::filesystem::symlink_status(source)))
            throw std::invalid_argument("Invalid downloaded package");
        snapshot.directory = directory_ / (".download-" + std::to_string(std::random_device{}()));
        if (!std::filesystem::create_directory(snapshot.directory))
            throw std::runtime_error("Cannot stage downloaded package");
        input = snapshot.directory / "package.awscene";
        std::ifstream original(source, std::ios::binary);
        std::ofstream copy(input, std::ios::binary);
        std::array<char, 65536> bytes{};
        std::uintmax_t total = 0;
        while (original) {
            original.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
            const auto count = original.gcount();
            if (count <= 0)
                break;
            total += static_cast<std::uintmax_t>(count);
            if (total > receipt->size)
                throw std::invalid_argument("Downloaded package exceeds receipt size");
            copy.write(bytes.data(), count);
        }
        copy.close();
        if (original.bad() || !copy || total != receipt->size ||
            packageSha256(input, receipt->size) != receipt->sha256)
            throw std::invalid_argument("Downloaded package size or SHA-256 mismatch");
    }
    auto package = loadScenePackage(input);
    const auto preview = package->images.at(package->scene.preview);
    if (preview.width > 2048 || preview.height > 2048)
        throw std::invalid_argument("Library preview must be at most 2048x2048 pixels");
    auto id = package->scene.id;
    for (auto& c : id)
        if (c >= 'A' && c <= 'F')
            c = static_cast<char>(c - 'A' + 'a');
    if (receipt && id != receipt->sceneId)
        throw std::invalid_argument("Downloaded package scene ID mismatch");
    Statement existing(database_, "SELECT id FROM scenes WHERE id=?");
    existing.bind(1, id);
    if (existing.step())
        throw std::invalid_argument("This scene ID is already installed");
    Statement count(database_, "SELECT count(*) FROM scenes");
    count.step();
    if (std::stoi(count.text(0)) >= 128)
        throw std::runtime_error("Local preview library limit (128 scenes) reached");
    const auto final = directory_ / "library" / id;
    const auto staging =
        directory_ / "library" / (".import-" + std::to_string(std::random_device{}()));
    if (!std::filesystem::create_directory(staging))
        throw std::runtime_error("Cannot reserve import staging directory");
    bool renamed = false;
    try {
        writeBytes(staging / "manifest.json",
                   std::span(reinterpret_cast<const std::uint8_t*>(package->manifest.data()),
                             package->manifest.size()));
        for (const auto& [name, bytes] : package->assets)
            writeBytes(staging / name, bytes);
        if (std::filesystem::exists(final))
            throw std::runtime_error("Import destination exists; not overwritten");
        std::filesystem::rename(staging, final);
        renamed = true;
        execute("BEGIN IMMEDIATE");
        try {
            Statement insert(database_,
                             "INSERT INTO scenes(id,title,author,license,preview,installed) "
                             "VALUES(?,?,?,?,?,unixepoch())");
            insert.bind(1, id);
            insert.bind(2, package->scene.title);
            insert.bind(3, package->scene.author);
            insert.bind(4, package->scene.license);
            insert.bind(5, package->scene.preview);
            insert.step();
            Statement history(
                database_,
                "INSERT INTO history(scene,action,recorded) VALUES(?,'import',unixepoch())");
            history.bind(1, id);
            history.step();
            if (receipt) {
                Statement download(database_,
                                   "INSERT INTO downloads(scene,sha256,source,bytes,recorded) "
                                   "VALUES(?,?,?,?,unixepoch())");
                download.bind(1, id);
                download.bind(2, receipt->sha256);
                download.bind(3, receipt->source);
                download.bind(4, std::to_string(receipt->size));
                download.step();
            }
            execute("COMMIT");
        } catch (...) {
            execute("ROLLBACK");
            throw;
        }
    } catch (...) {
        std::error_code ignored;
        std::filesystem::remove_all(renamed ? final : staging, ignored);
        throw;
    }
    return id;
}
void LibraryStore::favorite(const std::string& id, bool enabled) {
    scenePath(id);
    Statement query(database_, "UPDATE scenes SET favorite=? WHERE id=?");
    query.bind(1, enabled ? "1" : "0");
    query.bind(2, id);
    query.step();
}
void LibraryStore::assign(const std::vector<std::string>& monitors, const std::string& id) {
    scenePath(id);
    if (monitors.empty() || monitors.size() > 16)
        throw std::invalid_argument("Expected 1..16 monitors");
    execute("BEGIN IMMEDIATE");
    try {
        for (const auto& monitor : monitors) {
            Statement query(database_, "INSERT INTO assignments(monitor,scene) VALUES(?,?) ON "
                                       "CONFLICT(monitor) DO UPDATE SET scene=excluded.scene");
            query.bind(1, monitor);
            query.bind(2, id);
            query.step();
        }
        execute("COMMIT");
    } catch (...) {
        execute("ROLLBACK");
        throw;
    }
}
Json LibraryStore::assignments() const {
    Statement query(database_, "SELECT monitor,scene FROM assignments ORDER BY monitor");
    Json result = Json::object();
    while (query.step())
        result[query.text(0)] = query.text(1);
    return result;
}
void LibraryStore::remove(const std::string& id) {
    const auto path = scenePath(id);
    Statement assigned(database_, "SELECT monitor FROM assignments WHERE scene=?");
    assigned.bind(1, id);
    if (assigned.step())
        throw std::invalid_argument("Scene is assigned to a monitor; apply another scene first");
    const auto trash =
        directory_ / "library" / (".removed-" + std::to_string(std::random_device{}()));
    if (std::filesystem::exists(path))
        std::filesystem::rename(path, trash);
    try {
        Statement query(database_, "DELETE FROM scenes WHERE id=?");
        query.bind(1, id);
        query.step();
    } catch (...) {
        if (std::filesystem::exists(trash))
            std::filesystem::rename(trash, path);
        throw;
    }
    std::error_code ignored;
    std::filesystem::remove_all(trash, ignored);
}
Json LibraryStore::downloadHistory() const {
    Statement query(
        database_,
        "SELECT scene,sha256,source,bytes,recorded FROM downloads ORDER BY id DESC LIMIT 100");
    Json result = Json::array();
    while (query.step())
        result.push_back({{"sceneId", query.text(0)},
                          {"sha256", query.text(1)},
                          {"source", query.text(2)},
                          {"bytes", std::stoull(query.text(3))},
                          {"recorded", query.text(4)}});
    return result;
}
} // namespace aetherwall
