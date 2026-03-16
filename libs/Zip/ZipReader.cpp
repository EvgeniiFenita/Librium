#include "ZipReader.hpp"
#include <zip.h>
#include <stdexcept>

namespace {

struct CZipArchive
{
    zip_t* archive{nullptr};

    explicit CZipArchive(const std::string& path) 
{
        int err = 0;
        archive = zip_open(path.c_str(), ZIP_RDONLY, &err);
        if (!archive)
            throw Librium::Zip::CZipError("Cannot open zip: " + path);
    }

    ~CZipArchive() 
{
        if (archive) zip_close(archive);
    }

    CZipArchive(const CZipArchive&)            = delete;
    CZipArchive& operator=(const CZipArchive&) = delete;
};

Librium::Zip::CZipEntry MakeEntry(zip_stat_t& stat) 
{
    Librium::Zip::CZipEntry e;
    if (stat.valid & ZIP_STAT_NAME) e.name             = stat.name;
    if (stat.valid & ZIP_STAT_SIZE) e.uncompressedSize = stat.size;
    if (stat.valid & ZIP_STAT_COMP_SIZE) e.compressedSize = stat.comp_size;

    e.isDirectory = false;
    if (!e.name.empty() && e.name.back() == '/')
        e.isDirectory = true;

    return e;
}

std::vector<uint8_t> ReadCurrentEntry(zip_t* archive, zip_uint64_t index, zip_uint64_t size) 
{
    zip_file_t* zf = zip_fopen_index(archive, index, 0);
    if (!zf) throw Librium::Zip::CZipError("Cannot open entry inside zip");

    std::vector<uint8_t> buf(static_cast<size_t>(size));
    zip_int64_t bytes_read = zip_fread(zf, buf.data(), buf.size());
    zip_fclose(zf);

    if (bytes_read < 0 || static_cast<zip_uint64_t>(bytes_read) != size)
        throw Librium::Zip::CZipError("Failed to read whole entry data");

    return buf;
}

} // namespace

namespace Librium::Zip {

std::vector<CZipEntry> CZipReader::ListEntries(const std::string& zipPath) 
{
    CZipArchive za(zipPath);
    std::vector<CZipEntry> result;
    
    zip_int64_t num_entries = zip_get_num_entries(za.archive, 0);
    for (zip_int64_t i = 0; i < num_entries; ++i) 
{
        zip_stat_t stat;
        if (zip_stat_index(za.archive, i, 0, &stat) == 0) 
{
            result.push_back(MakeEntry(stat));
        }
    }
    return result;
}

std::vector<uint8_t> CZipReader::ReadEntry(const std::string& zipPath, const std::string& entryName) 
{
    CZipArchive za(zipPath);
    zip_stat_t stat;
    if (zip_stat(za.archive, entryName.c_str(), 0, &stat) != 0)
        throw CZipError("Entry not found: " + entryName);

    if (!(stat.valid & ZIP_STAT_SIZE))
        throw CZipError("Unknown size for entry: " + entryName);

    zip_int64_t index = zip_name_locate(za.archive, entryName.c_str(), 0);
    if (index < 0)
        throw CZipError("Cannot locate entry index: " + entryName);

    return ReadCurrentEntry(za.archive, static_cast<zip_uint64_t>(index), stat.size);
}

void CZipReader::IterateEntries(
    const std::string& zipPath,
    const std::function<bool(const std::string& name, std::vector<uint8_t> data)>& callback) 
{
    CZipArchive za(zipPath);
    zip_int64_t num_entries = zip_get_num_entries(za.archive, 0);

    for (zip_int64_t i = 0; i < num_entries; ++i) 
{
        zip_stat_t stat;
        if (zip_stat_index(za.archive, i, 0, &stat) == 0) 
{
            CZipEntry e = MakeEntry(stat);
            if (!e.isDirectory && (stat.valid & ZIP_STAT_SIZE)) 
{
                auto data = ReadCurrentEntry(za.archive, static_cast<zip_uint64_t>(i), stat.size);
                if (!callback(e.name, std::move(data))) 
{
                    break;
                }
            }
        }
    }
}

void CZipReader::IterateEntryNames(
    const std::string& zipPath,
    const std::function<bool(const CZipEntry& entry)>& callback) 
{
    CZipArchive za(zipPath);
    zip_int64_t num_entries = zip_get_num_entries(za.archive, 0);

    for (zip_int64_t i = 0; i < num_entries; ++i) 
{
        zip_stat_t stat;
        if (zip_stat_index(za.archive, i, 0, &stat) == 0) 
{
            if (!callback(MakeEntry(stat))) 
{
                break;
            }
        }
    }
}

} // namespace Librium::Zip






