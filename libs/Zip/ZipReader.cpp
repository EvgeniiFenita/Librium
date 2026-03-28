#include "ZipReader.hpp"
#include "Log/Logger.hpp"
#include "Utils/StringUtils.hpp"

#include <zip.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace Librium::Zip {

void SZipDeleter::operator()(struct zip* za) const
{
    if (za) zip_close(za);
}

namespace {
struct SZipFileDeleter
{
    void operator()(zip_file_t* zf) const
    {
        if (zf) zip_fclose(zf);
    }
};

using zip_file_ptr = std::unique_ptr<zip_file_t, SZipFileDeleter>;

bool IsDirectoryEntry(std::string_view name)
{
    return !name.empty() && (name.back() == '/' || name.back() == '\\');
}

SZipEntry MakeZipEntry(const zip_stat_t& stat)
{
    SZipEntry entry;
    entry.name = stat.name ? stat.name : "";
    entry.uncompressedSize = stat.size;
    entry.compressedSize = stat.comp_size;
    entry.isDirectory = IsDirectoryEntry(entry.name);
    return entry;
}

bool TryGetEntryStat(zip_t* archive, zip_int64_t index, zip_stat_t& stat)
{
    zip_stat_init(&stat);
    return zip_stat_index(archive, index, 0, &stat) == 0;
}

template<typename TCallback>
void ForEachEntry(zip_t* archive, TCallback&& callback)
{
    const zip_int64_t numEntries = zip_get_num_entries(archive, 0);
    for (zip_int64_t i = 0; i < numEntries; ++i)
    {
        zip_stat_t stat;
        if (!TryGetEntryStat(archive, i, stat))
        {
            continue;
        }

        if (!callback(i, stat))
        {
            break;
        }
    }
}
} // namespace

CZipReader::CZipReader(const std::filesystem::path& path)
    : m_path(path)
{
    const std::string pathStr = Utils::CStringUtils::PathToUtf8String(path);
    LOG_DEBUG("Opening zip handle: {}", pathStr);
    
    zip_t* za = nullptr;
#ifdef _WIN32
    zip_error_t zerr;
    zip_error_init(&zerr);
    zip_source_t* src = zip_source_win32w_create(path.c_str(), 0, -1, &zerr);
    if (!src)
    {
        std::string msg = zip_error_strerror(&zerr);
        zip_error_fini(&zerr);
        throw CZipError("Cannot create zip source for '" + std::string(pathStr) + "': " + msg);
    }
    za = zip_open_from_source(src, ZIP_RDONLY, &zerr);
    if (!za)
    {
        zip_source_free(src);
        std::string msg = zip_error_strerror(&zerr);
        zip_error_fini(&zerr);
        throw CZipError("Cannot open zip from source '" + std::string(pathStr) + "': " + msg);
    }
    m_za.reset(za);
    zip_error_fini(&zerr);
#else
    int err = 0;
    za = zip_open(path.c_str(), ZIP_RDONLY, &err);
    if (!za)
    {
        zip_error_t zerr;
        zip_error_init_with_code(&zerr, err);
        std::string msg = zip_error_strerror(&zerr);
        zip_error_fini(&zerr);
        throw CZipError("Cannot open zip '" + std::string(pathStr) + "': " + msg);
    }
    m_za.reset(za);
#endif
}

std::vector<uint8_t> CZipReader::ReadEntry(const std::string& entryName) const
{
    const std::string pathStr = Utils::CStringUtils::PathToUtf8String(m_path);

    zip_file_ptr zf(zip_fopen(m_za.get(), entryName.c_str(), 0));
    if (!zf)
    {
        throw CZipError("Cannot open entry '" + entryName + "' in " + pathStr);
    }

    struct zip_stat st;
    zip_stat_init(&st);
    if (zip_stat(m_za.get(), entryName.c_str(), 0, &st) != 0)
    {
        throw CZipError("Cannot stat entry '" + entryName + "' in " + pathStr);
    }

    std::vector<uint8_t> buffer(st.size);
    auto bytesRead = zip_fread(zf.get(), buffer.data(), st.size);
    if (bytesRead < 0 || static_cast<zip_uint64_t>(bytesRead) != st.size)
    {
        throw CZipError("Failed to read entire entry '" + entryName + "' in " + pathStr);
    }

    return buffer;
}

std::vector<SZipEntry> CZipReader::ListEntries(const std::filesystem::path& zipPath)
{
    std::vector<SZipEntry> result;
    CZipReader reader(zipPath);

    ForEachEntry(reader.m_za.get(), [&](zip_int64_t, const zip_stat_t& stat)
    {
        result.push_back(MakeZipEntry(stat));
        return true;
    });
    return result;
}

std::vector<uint8_t> CZipReader::ReadEntry(const std::filesystem::path& zipPath, const std::string& entryName)
{
    CZipReader reader(zipPath);
    return reader.ReadEntry(entryName);
}

void CZipReader::IterateEntries(const std::filesystem::path& zipPath, const std::function<bool(const std::string&, std::vector<uint8_t>)>& callback)
{
    CZipReader reader(zipPath);
    ForEachEntry(reader.m_za.get(), [&](zip_int64_t index, const zip_stat_t& stat)
    {
        const SZipEntry entry = MakeZipEntry(stat);
        if (entry.isDirectory)
        {
            return true;
        }

        zip_file_ptr zf(zip_fopen_index(reader.m_za.get(), index, 0));
        if (!zf)
        {
            return true;
        }

        std::vector<uint8_t> data(stat.size);
        const auto bytesRead = zip_fread(zf.get(), data.data(), stat.size);
        zf.reset(); // close early
        if (bytesRead < 0 || static_cast<zip_uint64_t>(bytesRead) != stat.size)
        {
            return true;
        }

        return callback(entry.name, std::move(data));
    });
}

void CZipReader::IterateEntryNames(const std::filesystem::path& zipPath, const std::function<bool(const SZipEntry&)>& callback)
{
    CZipReader reader(zipPath);
    ForEachEntry(reader.m_za.get(), [&](zip_int64_t, const zip_stat_t& stat)
    {
        return callback(MakeZipEntry(stat));
    });
}

} // namespace Librium::Zip
