#pragma once

#include <filesystem>
#include <string>
#include <map>
#include <stdexcept>
#include <zip.h>
#include <random>

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
    int err = 0;
    zip_t* archive = nullptr;

#ifdef _WIN32
    zip_error_t zerr;
    zip_error_init(&zerr);
    // On Windows we use the wide char API to create/open
    archive = zip_open(reinterpret_cast<const char*>(zipPath.u8string().c_str()), ZIP_CREATE | ZIP_TRUNCATE, &err);
    // Wait, zip_open on windows with libzip from vcpkg might not support UTF-8 path automatically unless specialized.
    // However, for tests with temp paths, usually it works if no non-ascii.
    // To be safe and follow ZipReader pattern:
    zip_source_t* src = zip_source_win32w_create(zipPath.c_str(), 0, -1, &zerr);
    if (src) {
        archive = zip_open_from_source(src, ZIP_CREATE | ZIP_TRUNCATE, &zerr);
    }
#else
    archive = zip_open(zipPath.c_str(), ZIP_CREATE | ZIP_TRUNCATE, &err);
#endif

    if (!archive)
    {
        throw std::runtime_error("Failed to create test zip");
    }

    for (const auto& [name, content] : files)
    {
        zip_source_t* source = zip_source_buffer(archive, content.c_str(), content.size(), 0);
        if (!source)
        {
            zip_close(archive);
            throw std::runtime_error("Failed to create zip source for " + name);
        }

        if (zip_file_add(archive, name.c_str(), source, ZIP_FL_OVERWRITE | ZIP_FL_ENC_UTF_8) < 0)
        {
            zip_source_free(source);
            zip_close(archive);
            throw std::runtime_error("Failed to add file to zip: " + name);
        }
    }

    if (zip_close(archive) < 0)
    {
        throw std::runtime_error("Failed to close test zip");
    }
}

} // namespace Librium::Tests
