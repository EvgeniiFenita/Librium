#pragma once

#include <filesystem>
#include <string>
#include <map>
#include <stdexcept>
#include <random>
#include "Zip/ZipReader.hpp"
#include <zip.h>

namespace Librium::Tests {

class CTempDir
{
public:
    CTempDir()
    {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(10000, 99999);
        
        m_path = std::filesystem::temp_directory_path() / ("librium_test_" + std::to_string(dis(gen)));
        std::filesystem::create_directories(m_path);
    }

    ~CTempDir()
    {
        if (std::filesystem::exists(m_path))
        {
            std::filesystem::remove_all(m_path);
        }
    }

    [[nodiscard]] std::filesystem::path GetPath() const
    {
        return m_path;
    }

private:
    std::filesystem::path m_path;
};

inline void CreateTestZip(const std::filesystem::path& zipPath, const std::map<std::string, std::string>& files)
{
    zip_t* raw_archive = nullptr;

#ifdef _WIN32
    zip_error_t zerr;
    zip_error_init(&zerr);
    zip_source_t* src = zip_source_win32w_create(zipPath.c_str(), 0, -1, &zerr);
    if (src) {
        raw_archive = zip_open_from_source(src, ZIP_CREATE | ZIP_TRUNCATE, &zerr);
    }
    zip_error_fini(&zerr);
#else
    int err = 0;
    raw_archive = zip_open(zipPath.c_str(), ZIP_CREATE | ZIP_TRUNCATE, &err);
#endif

    if (!raw_archive)
    {
        throw std::runtime_error("Failed to create test zip");
    }

    Zip::zip_ptr archive(raw_archive);

    for (const auto& [name, content] : files)
    {
        zip_source_t* source = zip_source_buffer(archive.get(), content.c_str(), content.size(), 0);
        if (!source)
        {
            throw std::runtime_error("Failed to create zip source for " + name);
        }

        if (zip_file_add(archive.get(), name.c_str(), source, ZIP_FL_OVERWRITE | ZIP_FL_ENC_UTF_8) < 0)
        {
            zip_source_free(source);
            throw std::runtime_error("Failed to add file to zip: " + name);
        }
    }
}

} // namespace Librium::Tests
