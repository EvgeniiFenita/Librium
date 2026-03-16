#pragma once

#include "Inpx/BookRecord.hpp"

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace LibIndexer::Config {

class CConfigError : public std::runtime_error
{
public:
    explicit CConfigError(const std::string& msg) : std::runtime_error(msg) {}
};

struct DatabaseConfig   { std::string path{"library.db"}; };
struct LibraryConfig    { std::string inpxPath; std::string archivesDir; };

struct ImportConfig
{
    bool        parseFb2{true};
    int         threadCount{4};
    size_t      transactionBatchSize{1000};
    std::string mode{"full"};
};

struct FiltersConfig
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

struct LoggingConfig
{
    std::string level{"info"};
    std::string file{""};
    size_t      progressInterval{1000};
};

class CAppConfig
{
public:
    DatabaseConfig database;
    LibraryConfig  library;
    ImportConfig   import;
    FiltersConfig  filters;
    LoggingConfig  logging;

    [[nodiscard]] static CAppConfig Defaults();
    [[nodiscard]] static CAppConfig Load(const std::string& path);
    void Save(const std::string& path) const;
};

class CBookFilter
{
public:
    explicit CBookFilter(const FiltersConfig& cfg);
    [[nodiscard]] bool ShouldInclude(const Inpx::BookRecord& record) const;
    [[nodiscard]] const std::string& LastReason() const { return m_lastReason; }

private:
    FiltersConfig       m_cfg;
    mutable std::string m_lastReason;
};

} // namespace LibIndexer::Config
