#include "ZipReader.hpp"
#include "Log/Logger.hpp"

#include <zip.h>
#include <cstring>

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
    struct SZipFileDeleter {
        void operator()(zip_file_t* zf) const {
            if (zf) zip_fclose(zf);
        }
    };
    using zip_file_ptr = std::unique_ptr<zip_file_t, SZipFileDeleter>;
}

CZipReader::CZipReader(const std::filesystem::path& path)
    : m_path(path)
{
    auto u8path = path.u8string();
    auto pathStr = reinterpret_cast<const char*>(u8path.c_str());
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
#endif
    m_za.reset(za);
}

std::vector<uint8_t> CZipReader::ReadEntry(const std::string& entryName) const
{
    zip_file_ptr zf(zip_fopen(m_za.get(), entryName.c_str(), 0));
    if (!zf)
    {
        throw CZipError("Cannot open entry '" + entryName + "' in " + reinterpret_cast<const char*>(m_path.u8string().c_str()));
    }

    struct zip_stat st;
    zip_stat_init(&st);
    if (zip_stat(m_za.get(), entryName.c_str(), 0, &st) != 0)
    {
        throw CZipError("Cannot stat entry '" + entryName + "' in " + reinterpret_cast<const char*>(m_path.u8string().c_str()));
    }

    std::vector<uint8_t> buffer(st.size);
    auto bytesRead = zip_fread(zf.get(), buffer.data(), st.size);
    if (bytesRead < 0 || static_cast<zip_uint64_t>(bytesRead) != st.size)
    {
        throw CZipError("Failed to read entire entry '" + entryName + "' in " + reinterpret_cast<const char*>(m_path.u8string().c_str()));
    }

    return buffer;
}

std::vector<SZipEntry> CZipReader::ListEntries(const std::filesystem::path& zipPath)
{
    std::vector<SZipEntry> result;
    CZipReader reader(zipPath);

    zip_int64_t num = zip_get_num_entries(reader.m_za.get(), 0);
    for (zip_int64_t i = 0; i < num; ++i)
    {
        struct zip_stat st;
        zip_stat_init(&st);
        if (zip_stat_index(reader.m_za.get(), i, 0, &st) == 0)
        {
            SZipEntry e;
            e.name = st.name;
            e.uncompressedSize = st.size;
            e.compressedSize = st.comp_size;
            e.isDirectory = (e.name.back() == '/' || e.name.back() == '\\');
            result.push_back(std::move(e));
        }
    }
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
    zip_int64_t num = zip_get_num_entries(reader.m_za.get(), 0);
    for (zip_int64_t i = 0; i < num; ++i)
    {
        struct zip_stat st;
        zip_stat_init(&st);
        if (zip_stat_index(reader.m_za.get(), i, 0, &st) == 0)
        {
            if (st.name[strlen(st.name) - 1] == '/' || st.name[strlen(st.name) - 1] == '\\')
            {
                continue;
            }

            zip_file_ptr zf(zip_fopen_index(reader.m_za.get(), i, 0));
            if (zf)
            {
                std::vector<uint8_t> data(st.size);
                auto bytesRead = zip_fread(zf.get(), data.data(), st.size);
                zf.reset(); // close early
                if (bytesRead < 0 || static_cast<zip_uint64_t>(bytesRead) != st.size)
                {
                    continue;
                }
                if (!callback(st.name, std::move(data)))
                {
                    break;
                }
            }
        }
    }
}

void CZipReader::IterateEntryNames(const std::filesystem::path& zipPath, const std::function<bool(const SZipEntry&)>& callback)
{
    CZipReader reader(zipPath);
    zip_int64_t num = zip_get_num_entries(reader.m_za.get(), 0);
    for (zip_int64_t i = 0; i < num; ++i)
    {
        struct zip_stat st;
        zip_stat_init(&st);
        if (zip_stat_index(reader.m_za.get(), i, 0, &st) == 0)
        {
            SZipEntry e;
            e.name = st.name;
            e.uncompressedSize = st.size;
            e.compressedSize = st.comp_size;
            e.isDirectory = (e.name.back() == '/' || e.name.back() == '\\');
            if (!callback(e))
            {
                break;
            }
        }
    }
}

} // namespace Librium::Zip
