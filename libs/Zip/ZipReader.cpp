#include "ZipReader.hpp"

#include <zip.h>

namespace Librium::Zip {

namespace {

struct SZipArchive
{
    zip_t* za = nullptr;
    explicit SZipArchive(const std::string& path)
    {
        int err = 0;
        za = zip_open(path.c_str(), ZIP_RDONLY, &err);
        if (!za)
        {
            zip_error_t zerr;
            zip_error_init_with_code(&zerr, err);
            std::string msg = zip_error_strerror(&zerr);
            zip_error_fini(&zerr);
            throw CZipError("Cannot open zip '" + path + "': " + msg);
        }
    }
    ~SZipArchive()
    {
        if (za) zip_close(za);
    }
};

} // namespace

std::vector<SZipEntry> CZipReader::ListEntries(const std::string& zipPath)
{
    std::vector<SZipEntry> result;
    SZipArchive arch(zipPath);

    zip_int64_t num = zip_get_num_entries(arch.za, 0);
    for (zip_int64_t i = 0; i < num; ++i)
    {
        struct zip_stat st;
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

std::vector<uint8_t> CZipReader::ReadEntry(const std::string& zipPath, const std::string& entryName)
{
    SZipArchive arch(zipPath);
    zip_file_t* zf = zip_fopen(arch.za, entryName.c_str(), 0);
    if (!zf)
    {
        throw CZipError("Cannot open entry '" + entryName + "' in " + zipPath);
    }

    struct zip_stat st;
    zip_stat(arch.za, entryName.c_str(), 0, &st);

    std::vector<uint8_t> buffer(st.size);
    zip_fread(zf, buffer.data(), st.size);
    zip_fclose(zf);

    return buffer;
}

void CZipReader::IterateEntries(const std::string& zipPath, const std::function<bool(const std::string&, std::vector<uint8_t>)>& callback)
{
    SZipArchive arch(zipPath);
    zip_int64_t num = zip_get_num_entries(arch.za, 0);
    for (zip_int64_t i = 0; i < num; ++i)
    {
        struct zip_stat st;
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
                zip_fread(zf, data.data(), st.size);
                zip_fclose(zf);
                if (!callback(st.name, std::move(data)))
                {
                    break;
                }
            }
        }
    }
}

void CZipReader::IterateEntryNames(const std::string& zipPath, const std::function<bool(const SZipEntry&)>& callback)
{
    SZipArchive arch(zipPath);
    zip_int64_t num = zip_get_num_entries(arch.za, 0);
    for (zip_int64_t i = 0; i < num; ++i)
    {
        struct zip_stat st;
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
