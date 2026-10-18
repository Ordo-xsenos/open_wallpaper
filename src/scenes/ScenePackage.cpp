#include "scenes/ScenePackage.h"
#include "scenes/SceneParser.h"

#include <archive.h>
#include <archive_entry.h>
#include <webp/decode.h>

#include <algorithm>
#include <array>
#include <fstream>
#include <limits>
#include <set>
#include <stdexcept>

namespace aetherwall {
namespace {
Bytes readFile(const std::filesystem::path& path, std::size_t limit) {
    const auto size = std::filesystem::file_size(path);
    if (size == 0 || size > limit)
        throw std::invalid_argument("File size exceeds limit or is empty");
    Bytes bytes(static_cast<std::size_t>(size));
    std::ifstream stream(path, std::ios::binary);
    if (!stream.read(reinterpret_cast<char*>(bytes.data()),
                     static_cast<std::streamsize>(bytes.size())) ||
        stream.peek() != std::char_traits<char>::eof()) {
        throw std::invalid_argument("Cannot read complete stable scene file");
    }
    return bytes;
}

void allowedFile(const std::string& path) {
    if (path == "manifest.json")
        return;
    if (!path.starts_with("assets/"))
        throw std::invalid_argument("Files must be in assets/");
    const auto dot = path.find_last_of('.');
    if (dot == std::string::npos)
        throw std::invalid_argument("Missing asset extension");
    const auto extension = path.substr(dot);
    if (extension != ".png" && extension != ".jpg" && extension != ".jpeg" && extension != ".webp")
        throw std::invalid_argument("Only PNG, JPEG and static WebP assets are permitted");
}

void insertFile(std::map<std::string, Bytes>& files, std::string name, Bytes bytes,
                std::size_t& total) {
    allowedFile(name);
    if (name == "manifest.json" && bytes.size() > SceneLimits::manifestBytes)
        throw std::invalid_argument("Manifest exceeds size limit");
    if (bytes.empty() || bytes.size() > SceneLimits::expandedBytes - total)
        throw std::invalid_argument("Expanded package budget exceeded or empty file");
    total += bytes.size();
    if (!files.emplace(std::move(name), std::move(bytes)).second)
        throw std::invalid_argument("Duplicate/case-colliding file path");
}

std::map<std::string, Bytes> readDirectory(const std::filesystem::path& root) {
    if (std::filesystem::is_symlink(std::filesystem::symlink_status(root)))
        throw std::invalid_argument("Scene directory must not be a symlink");
    const auto canonicalRoot = std::filesystem::canonical(root);
    std::map<std::string, Bytes> files;
    std::size_t total = 0;
    int entries = 0;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
        if (++entries > SceneLimits::entries)
            throw std::invalid_argument("Too many package entries");
        const auto status = entry.symlink_status();
        if (std::filesystem::is_symlink(status))
            throw std::invalid_argument("Package symlinks are forbidden");
        const auto relative = entry.path().lexically_relative(root).generic_string();
        const auto name = normalizedAssetPath(relative);
        const auto resolved =
            std::filesystem::canonical(entry.path()).lexically_relative(canonicalRoot);
        if (resolved.empty() || *resolved.begin() == "..")
            throw std::invalid_argument("Directory entry escapes scene root");
        if (std::filesystem::is_directory(status)) {
            if (name != "assets" && !name.starts_with("assets/"))
                throw std::invalid_argument("Unexpected scene directory");
            continue;
        }
        if (!std::filesystem::is_regular_file(status) ||
            std::filesystem::hard_link_count(entry.path()) != 1)
            throw std::invalid_argument("Package entries must be ordinary unlinked files");
        allowedFile(name);
        insertFile(files, name,
                   readFile(entry.path(), std::min(SceneLimits::packageBytes,
                                                   SceneLimits::expandedBytes - total)),
                   total);
    }
    return files;
}

std::map<std::string, Bytes> readZip(const std::filesystem::path& path) {
    const auto encoded = readFile(path, SceneLimits::packageBytes);
    using Reader = std::unique_ptr<archive, decltype(&archive_read_free)>;
    Reader reader(archive_read_new(), &archive_read_free);
    if (!reader)
        throw std::bad_alloc();
    if (archive_read_support_filter_none(reader.get()) != ARCHIVE_OK ||
        archive_read_support_format_zip(reader.get()) != ARCHIVE_OK ||
        archive_read_open_memory(reader.get(), encoded.data(), encoded.size()) != ARCHIVE_OK)
        throw std::invalid_argument("Cannot open ZIP scene package");
    std::map<std::string, Bytes> files;
    std::set<std::string> entries;
    std::size_t total = 0;
    archive_entry* entry = nullptr;
    int result = ARCHIVE_OK;
    while ((result = archive_read_next_header(reader.get(), &entry)) == ARCHIVE_OK) {
        const char* rawName = archive_entry_pathname_utf8(entry);
        if (!rawName)
            throw std::invalid_argument("ZIP path is not valid UTF-8");
        std::string name(rawName);
        const bool directory = archive_entry_filetype(entry) == AE_IFDIR;
        if (directory && !name.empty() && name.back() == '/')
            name.pop_back();
        name = normalizedAssetPath(name);
        if (entries.size() >= SceneLimits::entries || !entries.insert(name).second)
            throw std::invalid_argument("Too many ZIP entries or duplicate path");
        if (archive_entry_symlink(entry) || archive_entry_hardlink(entry) ||
            archive_entry_is_encrypted(entry) != 0)
            throw std::invalid_argument("ZIP links/encryption are forbidden");
        if (directory) {
            if (name != "assets" && !name.starts_with("assets/"))
                throw std::invalid_argument("Unexpected ZIP directory");
            continue;
        }
        if (archive_entry_filetype(entry) != AE_IFREG)
            throw std::invalid_argument("Unsupported ZIP entry type");
        allowedFile(name);
        const auto declared = archive_entry_size(entry);
        const auto limit =
            name == "manifest.json" ? SceneLimits::manifestBytes : SceneLimits::packageBytes;
        if (!archive_entry_size_is_set(entry) || declared <= 0 ||
            static_cast<std::uint64_t>(declared) > limit ||
            static_cast<std::uint64_t>(declared) > SceneLimits::expandedBytes - total)
            throw std::invalid_argument("ZIP entry size exceeds budget");
        Bytes bytes;
        std::array<std::uint8_t, 16384> chunk{};
        la_ssize_t count = 0;
        while ((count = archive_read_data(reader.get(), chunk.data(), chunk.size())) > 0) {
            const auto amount = static_cast<std::size_t>(count);
            if (amount > limit - bytes.size() ||
                amount > SceneLimits::expandedBytes - total - bytes.size())
                throw std::invalid_argument("ZIP decompression exceeds budget");
            bytes.insert(bytes.end(), chunk.begin(),
                         chunk.begin() + static_cast<std::ptrdiff_t>(amount));
        }
        if (count < 0 || bytes.size() != static_cast<std::uint64_t>(declared))
            throw std::invalid_argument("ZIP entry damaged, CRC failed or length changed");
        insertFile(files, name, std::move(bytes), total);
        if (total > encoded.size() * 200 + 1024 * 1024)
            throw std::invalid_argument("ZIP compression ratio exceeds limit");
    }
    if (result != ARCHIVE_EOF || archive_read_close(reader.get()) != ARCHIVE_OK)
        throw std::invalid_argument("Corrupt ZIP directory");
    return files;
}

std::uint32_t bigEndian(std::span<const std::uint8_t> data, std::size_t offset) {
    return (static_cast<std::uint32_t>(data[offset]) << 24) |
           (static_cast<std::uint32_t>(data[offset + 1]) << 16) |
           (static_cast<std::uint32_t>(data[offset + 2]) << 8) | data[offset + 3];
}
} // namespace

ImageInfo inspectImage(std::string_view filename, std::span<const std::uint8_t> bytes) {
    ImageInfo info{};
    const auto dot = filename.find_last_of('.');
    if (dot == std::string_view::npos)
        throw std::invalid_argument("Missing image extension");
    const auto extension = filename.substr(dot);
    const std::array<std::uint8_t, 8> png{137, 80, 78, 71, 13, 10, 26, 10};
    if (extension == ".png" && bytes.size() >= 33 &&
        std::equal(png.begin(), png.end(), bytes.begin()) && bigEndian(bytes, 8) == 13 &&
        std::equal(bytes.begin() + 12, bytes.begin() + 16, "IHDR")) {
        const auto width = bigEndian(bytes, 16);
        const auto height = bigEndian(bytes, 20);
        if (width > SceneLimits::imageDimension || height > SceneLimits::imageDimension)
            throw std::invalid_argument("PNG dimensions exceed limit");
        info = {static_cast<int>(width), static_cast<int>(height)};
    } else if (extension == ".webp") {
        WebPBitstreamFeatures features{};
        if (WebPGetFeatures(bytes.data(), bytes.size(), &features) != VP8_STATUS_OK ||
            features.has_animation)
            throw std::invalid_argument("Invalid or animated WebP");
        info = {features.width, features.height};
    } else if ((extension == ".jpg" || extension == ".jpeg") && bytes.size() > 4 &&
               bytes[0] == 0xff && bytes[1] == 0xd8) {
        std::size_t offset = 2;
        while (offset + 4 <= bytes.size()) {
            if (bytes[offset++] != 0xff)
                break;
            while (offset < bytes.size() && bytes[offset] == 0xff)
                ++offset;
            if (offset + 3 > bytes.size())
                break;
            const auto marker = bytes[offset++];
            if (marker == 0xda || marker == 0xd9)
                break;
            const std::size_t length =
                static_cast<std::size_t>(bytes[offset]) * 256 + bytes[offset + 1];
            if (length < 2 || length > bytes.size() - offset)
                break;
            if (marker == 0xc0 || marker == 0xc1 || marker == 0xc2) {
                if (length < 8)
                    break;
                info.height = bytes[offset + 3] * 256 + bytes[offset + 4];
                info.width = bytes[offset + 5] * 256 + bytes[offset + 6];
                break;
            }
            offset += length;
        }
    }
    if (info.width <= 0 || info.height <= 0 || info.width > SceneLimits::imageDimension ||
        info.height > SceneLimits::imageDimension)
        throw std::invalid_argument("Invalid, mismatched or oversized image header");
    return info;
}

std::shared_ptr<const ScenePackage> loadScenePackage(const std::filesystem::path& path) {
    std::map<std::string, Bytes> files;
    if (std::filesystem::is_directory(path))
        files = readDirectory(path);
    else {
        if (path.extension() != ".awscene" ||
            std::filesystem::is_symlink(std::filesystem::symlink_status(path)))
            throw std::invalid_argument("Expected a scene directory or ordinary .awscene file");
        files = readZip(path);
    }
    const auto manifest = files.find("manifest.json");
    if (manifest == files.end())
        throw std::invalid_argument("manifest.json is missing");
    auto package = std::make_shared<ScenePackage>();
    package->scene = parseSceneManifest(std::string_view(
        reinterpret_cast<const char*>(manifest->second.data()), manifest->second.size()));
    package->manifest.assign(reinterpret_cast<const char*>(manifest->second.data()),
                             manifest->second.size());
    files.erase(manifest);
    std::size_t decoded = 0;
    for (auto& [name, bytes] : files) {
        const auto info = inspectImage(name, bytes);
        const auto size =
            static_cast<std::size_t>(info.width) * static_cast<std::size_t>(info.height) * 4;
        if (size > SceneLimits::decodedBytes - decoded)
            throw std::invalid_argument("Decoded texture memory budget exceeded");
        decoded += size;
        package->images.emplace(name, info);
        package->assets.emplace(name, std::move(bytes));
    }
    if (!package->images.contains(package->scene.preview))
        throw std::invalid_argument("Preview image is missing");
    for (const auto& layer : package->scene.layers) {
        if (layer.type == LayerType::Image && !package->images.contains(layer.source))
            throw std::invalid_argument("Layer image is missing: " + layer.source);
    }
    return package;
}
} // namespace aetherwall
