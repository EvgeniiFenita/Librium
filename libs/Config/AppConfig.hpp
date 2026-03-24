#pragma once

#include "Inpx/BookRecord.hpp"
#include "Utils/StringUtils.hpp"

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>
#include <filesystem>

namespace Librium::Config {

class CAppPaths
{
public:
    [[nodiscard]] static std::filesystem::path GetBookMetaDir(
        const std::filesystem::path& dbPath, int64_t id);
};

struct SFilterResult
{
    bool        included{true};
    std::string reason;

    [[nodiscard]] explicit operator bool() const
    {
        return included;
    }
};

class CConfigError : public std::runtime_error
{
public:
    explicit CConfigError(const std::string& msg) : std::runtime_error(msg)
    {}
};

struct SDatabaseConfig
{ 
    std::string path{"library.db"}; 
};

struct SLibraryConfig
{ 
    std::string inpxPath; 
    std::string archivesDir; 
};

struct SImportConfig
{
    bool        parseFb2{true};
    int         threadCount{4};
    size_t      transactionBatchSize{1000};

    // SQLite performance tunables
    int64_t     sqliteCacheSize{-64000}; // negative for KB, positive for pages
    int64_t     sqliteMmapSize{268435456}; // 256MB
};

struct SFiltersConfig
{
    std::vector<std::string> excludeLanguages;
    std::vector<std::string> includeLanguages;
    std::vector<std::string> excludeGenres;
    std::vector<std::string> includeGenres;
    uint64_t                 minFileSize{0};
    uint64_t                 maxFileSize{0};
    std::vector<std::string> excludeAuthors;
    std::vector<std::string> excludeKeywords;
};

struct SLoggingConfig
{
    std::string level{"info"};
    std::string file;
    size_t      progressInterval{1000};
};

struct SAppConfig
{
    SDatabaseConfig database;
    SLibraryConfig  library;
    SImportConfig   import;
    SFiltersConfig  filters;
    SLoggingConfig  logging;

    [[nodiscard]] static SAppConfig Defaults();
    [[nodiscard]] static SAppConfig Load(const std::string& path);
    void Save(const std::string& path) const;
};

class CBookFilter
{
public:
    explicit CBookFilter(const SFiltersConfig& cfg);
    [[nodiscard]] SFilterResult ShouldInclude(const Inpx::SBookRecord& record) const;

private:
    SFiltersConfig      m_cfg;
};
} // namespace Librium::Config
