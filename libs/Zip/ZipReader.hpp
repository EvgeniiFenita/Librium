#pragma once

#include <string>
#include <vector>
#include <functional>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <stdexcept>

// Forward declarations for libzip types to avoid leaking zip.h into public interface
struct zip;

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

// Custom deleter for the main zip handle
struct SZipDeleter 
{
    void operator()(struct zip* za) const;
};

using zip_ptr = std::unique_ptr<struct zip, SZipDeleter>;

class CZipReader
{
public:
    // RAII Constructor: opens the zip archive
    explicit CZipReader(const std::filesystem::path& path);
    ~CZipReader() = default;

    CZipReader(const CZipReader&) = delete;
    CZipReader& operator=(const CZipReader&) = delete;

    // Instance methods for persistent archive access
    [[nodiscard]] std::vector<uint8_t> ReadEntry(const std::string& entryName) const;
    [[nodiscard]] const std::filesystem::path& Path() const { return m_path; }

    // Static helper methods for one-off operations
    [[nodiscard]] static std::vector<SZipEntry> ListEntries(const std::filesystem::path& zipPath);
    [[nodiscard]] static std::vector<uint8_t>   ReadEntry(const std::filesystem::path& zipPath, const std::string& entryName);

    static void IterateEntries(
        const std::filesystem::path& zipPath,
        const std::function<bool(const std::string&, std::vector<uint8_t>)>& callback);

    static void IterateEntryNames(
        const std::filesystem::path& zipPath,
        const std::function<bool(const SZipEntry&)>& callback);

private:
    zip_ptr               m_za;
    std::filesystem::path m_path;
};

} // namespace Librium::Zip
