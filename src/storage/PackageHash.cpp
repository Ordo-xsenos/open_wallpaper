#include "storage/PackageHash.h"
#include "scenes/Scene.h"
#include <array>
#include <fstream>
#include <stdexcept>
#ifdef _WIN32
#include <Windows.h>
#include <bcrypt.h>
#else
#include <memory>
#include <openssl/evp.h>
#endif

namespace aetherwall {
std::string packageSha256(const std::filesystem::path& path, std::uintmax_t expectedSize) {
    if (!expectedSize || expectedSize > SceneLimits::packageBytes ||
        std::filesystem::is_symlink(std::filesystem::symlink_status(path)) ||
        !std::filesystem::is_regular_file(path) || std::filesystem::file_size(path) != expectedSize)
        throw std::invalid_argument("Downloaded package size/type disagrees with catalog");
    std::ifstream file(path, std::ios::binary);
    if (!file)
        throw std::runtime_error("Cannot read downloaded package");
    std::array<unsigned char, 32> digest{};
#ifdef _WIN32
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    struct Cleanup {
        BCRYPT_ALG_HANDLE& algorithm;
        BCRYPT_HASH_HANDLE& hash;
        ~Cleanup() {
            if (hash)
                BCryptDestroyHash(hash);
            if (algorithm)
                BCryptCloseAlgorithmProvider(algorithm, 0);
        }
    } cleanup{algorithm, hash};
    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0 ||
        BCryptCreateHash(algorithm, &hash, nullptr, 0, nullptr, 0, 0) < 0)
        throw std::runtime_error("SHA-256 provider unavailable");
#else
    std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> hash(EVP_MD_CTX_new(), EVP_MD_CTX_free);
    if (!hash || EVP_DigestInit_ex(hash.get(), EVP_sha256(), nullptr) != 1)
        throw std::runtime_error("SHA-256 provider unavailable");
#endif
    std::array<unsigned char, 65536> buffer{};
    std::uintmax_t total = 0;
    while (file) {
        file.read(reinterpret_cast<char*>(buffer.data()),
                  static_cast<std::streamsize>(buffer.size()));
        const auto count = file.gcount();
        if (count <= 0)
            break;
        total += static_cast<std::uintmax_t>(count);
        if (total > expectedSize)
            throw std::invalid_argument("Package changed during validation");
#ifdef _WIN32
        if (BCryptHashData(hash, buffer.data(), static_cast<ULONG>(count), 0) < 0)
            throw std::runtime_error("SHA-256 update failed");
#else
        if (EVP_DigestUpdate(hash.get(), buffer.data(), static_cast<std::size_t>(count)) != 1)
            throw std::runtime_error("SHA-256 update failed");
#endif
    }
    if (file.bad() || total != expectedSize)
        throw std::invalid_argument("Package changed during validation");
#ifdef _WIN32
    if (BCryptFinishHash(hash, digest.data(), static_cast<ULONG>(digest.size()), 0) < 0)
        throw std::runtime_error("SHA-256 finalization failed");
#else
    unsigned int size = 0;
    if (EVP_DigestFinal_ex(hash.get(), digest.data(), &size) != 1 || size != digest.size())
        throw std::runtime_error("SHA-256 finalization failed");
#endif
    constexpr char hex[] = "0123456789abcdef";
    std::string result;
    result.reserve(64);
    for (auto byte : digest) {
        result += hex[byte >> 4];
        result += hex[byte & 15];
    }
    return result;
}
} // namespace aetherwall
