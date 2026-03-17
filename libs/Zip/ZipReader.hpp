#pragma once

#include <string>
#include <vector>
#include <functional>
#include <cstdint>
#include <filesystem>

namespace Librium::Zip {

struct SZipEntry
{
    std::string name;
    uint64_t    uncompressedSize{0};
    uint64_t    compressedSize{0};
    bool        isDirectory{false};
};

class CZipError : public std::runtime_error
{
public:
    explicit CZipError(const std::string& msg) : std::runtime_error(msg) {}
};

class CZipReader
{
public:
    [[nodiscard]] static std::vector<SZipEntry> ListEntries(const std::filesystem::path& zipPath);
    [[nodiscard]] static std::vector<uint8_t>   ReadEntry(const std::filesystem::path& zipPath, const std::string& entryName);

    static void IterateEntries(
        const std::filesystem::path& zipPath,
        const std::function<bool(const std::string&, std::vector<uint8_t>)>& callback);

    static void IterateEntryNames(
        const std::filesystem::path& zipPath,
        const std::function<bool(const SZipEntry&)>& callback);
};

} // namespace Librium::Zip
