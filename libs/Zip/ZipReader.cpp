#include "ZipReader.hpp"
#include "Log/Logger.hpp"

#include <zip.h>
#include <cstring>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace Librium::Zip {

namespace {

struct SZipArchive
{
    zip_t* za = nullptr;
    explicit SZipArchive(const std::filesystem::path& path)
    {
        auto u8path = path.u8string();
        auto pathStr = reinterpret_cast<const char*>(u8path.c_str());
        LOG_DEBUG("Opening zip archive: {}", pathStr);
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
    }
    ~SZipArchive()
    {
        if (za) zip_close(za);
    }
};

} // namespace

std::vector<SZipEntry> CZipReader::ListEntries(const std::filesystem::path& zipPath)
{
    std::vector<SZipEntry> result;
    SZipArchive arch(zipPath);

    zip_int64_t num = zip_get_num_entries(arch.za, 0);
    for (zip_int64_t i = 0; i < num; ++i)
    {
        struct zip_stat st;
        zip_stat_init(&st);
        if (zip_stat_index(arch.za, i, 0, &st) == 0)
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
    auto u8path = zipPath.u8string();
    auto pathStr = reinterpret_cast<const char*>(u8path.c_str());
    LOG_DEBUG("Reading entry '{}' from {}", entryName, pathStr);
    SZipArchive arch(zipPath);
    zip_file_t* zf = zip_fopen(arch.za, entryName.c_str(), 0);
    if (!zf)
    {
        throw CZipError("Cannot open entry '" + entryName + "' in " + std::string(pathStr));
    }

    struct zip_stat st;
    zip_stat_init(&st);
    if (zip_stat(arch.za, entryName.c_str(), 0, &st) != 0)
    {
        zip_fclose(zf);
        throw CZipError("Cannot stat entry '" + entryName + "' in " + std::string(pathStr));
    }

    std::vector<uint8_t> buffer(st.size);
    auto bytesRead = zip_fread(zf, buffer.data(), st.size);
    if (bytesRead < 0 || static_cast<zip_uint64_t>(bytesRead) != st.size)
    {
        zip_fclose(zf);
        throw CZipError("Failed to read entire entry '" + entryName + "' in " + std::string(pathStr));
    }
    zip_fclose(zf);

    return buffer;
}

void CZipReader::IterateEntries(const std::filesystem::path& zipPath, const std::function<bool(const std::string&, std::vector<uint8_t>)>& callback)
{
    SZipArchive arch(zipPath);
    zip_int64_t num = zip_get_num_entries(arch.za, 0);
    for (zip_int64_t i = 0; i < num; ++i)
    {
        struct zip_stat st;
        zip_stat_init(&st);
        if (zip_stat_index(arch.za, i, 0, &st) == 0)
        {
            if (st.name[strlen(st.name) - 1] == '/' || st.name[strlen(st.name) - 1] == '\\')
            {
                continue;
            }

            zip_file_t* zf = zip_fopen_index(arch.za, i, 0);
            if (zf)
            {
                std::vector<uint8_t> data(st.size);
                auto bytesRead = zip_fread(zf, data.data(), st.size);
                zip_fclose(zf);
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
    SZipArchive arch(zipPath);
    zip_int64_t num = zip_get_num_entries(arch.za, 0);
    for (zip_int64_t i = 0; i < num; ++i)
    {
        struct zip_stat st;
        zip_stat_init(&st);
        if (zip_stat_index(arch.za, i, 0, &st) == 0)
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
