#pragma once

#include <cstdint>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

namespace Librium::Zip {

struct CZipEntry
{
    std::string name;
    uint64_t    uncompressedSize{0};
    uint64_t    compressedSize{0};
    bool        isDirectory{false};
};

class CZipError : public std::runtime_error
{
public:
    explicit CZipError(const std::string& msg) : std::runtime_error(msg) 
{}
};

class CZipReader
{
public:
    [[nodiscard]] static std::vector<CZipEntry> ListEntries(const std::string& zipPath);

    [[nodiscard]] static std::vector<uint8_t> ReadEntry(
        const std::string& zipPath,
        const std::string& entryName);

    static void IterateEntries(
        const std::string& zipPath,
        const std::function<bool(const std::string& name,
                                 std::vector<uint8_t> data)>& callback);

    static void IterateEntryNames(
        const std::string& zipPath,
        const std::function<bool(const CZipEntry& entry)>& callback);
};

} // namespace Librium::Zip






