#include "AppConfig.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <fstream>
#include <thread>

using json = nlohmann::json;

namespace Librium::Config {

namespace {

template<typename TValue>
void LoadOptional(const json& object, const char* key, TValue& value)
{
    if (object.contains(key))
    {
        value = object[key].get<TValue>();
    }
}

json ToJson(const SDatabaseConfig& config)
{
    return {
        {"path", config.path}
    };
}

json ToJson(const SLibraryConfig& config)
{
    return {
        {"inpxPath", config.inpxPath},
        {"archivesDir", config.archivesDir}
    };
}

json ToJson(const SImportConfig& config)
{
    return {
        {"parseFb2", config.parseFb2},
        {"threadCount", config.threadCount},
        {"transactionBatchSize", config.transactionBatchSize},
        {"sqliteCacheSize", config.sqliteCacheSize},
        {"sqliteMmapSize", config.sqliteMmapSize}
    };
}

json ToJson(const SFiltersConfig& config)
{
    return {
        {"excludeLanguages", config.excludeLanguages},
        {"includeLanguages", config.includeLanguages},
        {"excludeGenres", config.excludeGenres},
        {"includeGenres", config.includeGenres},
        {"minFileSize", config.minFileSize},
        {"maxFileSize", config.maxFileSize},
        {"excludeAuthors", config.excludeAuthors},
        {"excludeKeywords", config.excludeKeywords}
    };
}

json ToJson(const SLoggingConfig& config)
{
    return {
        {"level", config.level},
        {"file", config.file},
        {"progressInterval", config.progressInterval}
    };
}

void LoadDatabaseConfig(const json& root, SDatabaseConfig& config)
{
    if (!root.contains("database"))
    {
        return;
    }

    const auto& db = root["database"];
    LoadOptional(db, "path", config.path);
}

void LoadLibraryConfig(const json& root, SLibraryConfig& config)
{
    if (!root.contains("library"))
    {
        return;
    }

    const auto& library = root["library"];
    LoadOptional(library, "inpxPath", config.inpxPath);
    LoadOptional(library, "archivesDir", config.archivesDir);
}

void LoadImportConfig(const json& root, SImportConfig& config)
{
    if (!root.contains("import"))
    {
        return;
    }

    const auto& import = root["import"];
    LoadOptional(import, "parseFb2", config.parseFb2);
    LoadOptional(import, "threadCount", config.threadCount);
    LoadOptional(import, "transactionBatchSize", config.transactionBatchSize);
    LoadOptional(import, "sqliteCacheSize", config.sqliteCacheSize);
    LoadOptional(import, "sqliteMmapSize", config.sqliteMmapSize);
}

void LoadFiltersConfig(const json& root, SFiltersConfig& config)
{
    if (!root.contains("filters"))
    {
        return;
    }

    const auto& filters = root["filters"];
    LoadOptional(filters, "excludeLanguages", config.excludeLanguages);
    LoadOptional(filters, "includeLanguages", config.includeLanguages);
    LoadOptional(filters, "excludeGenres", config.excludeGenres);
    LoadOptional(filters, "includeGenres", config.includeGenres);
    LoadOptional(filters, "excludeAuthors", config.excludeAuthors);
    LoadOptional(filters, "excludeKeywords", config.excludeKeywords);
    LoadOptional(filters, "minFileSize", config.minFileSize);
    LoadOptional(filters, "maxFileSize", config.maxFileSize);
}

void LoadLoggingConfig(const json& root, SLoggingConfig& config)
{
    if (!root.contains("logging"))
    {
        return;
    }

    const auto& logging = root["logging"];
    LoadOptional(logging, "level", config.level);
    LoadOptional(logging, "file", config.file);
    LoadOptional(logging, "progressInterval", config.progressInterval);
}

json ToJson(const SAppConfig& config)
{
    return {
        {"database", ToJson(config.database)},
        {"library", ToJson(config.library)},
        {"import", ToJson(config.import)},
        {"filters", ToJson(config.filters)},
        {"logging", ToJson(config.logging)}
    };
}

} // namespace

SAppConfig SAppConfig::Defaults() 
{
    SAppConfig c;
    c.import.threadCount = static_cast<int>(
        std::max(1u, std::thread::hardware_concurrency()));
    return c;
}

SAppConfig SAppConfig::Load(const std::string& path) 
{
    // Use filesystem::path to handle UTF-8 paths correctly on Windows
    std::ifstream f(Utils::CStringUtils::Utf8ToPath(path));
    if (!f.is_open())
        throw CConfigError("Cannot open config: " + path);

    json j;
    try 
    { 
        j = json::parse(f); 
    }
    catch (const json::parse_error& e) 
    { 
        throw CConfigError(std::string("JSON parse error: ") + e.what()); 
    }

    SAppConfig cfg = Defaults();

    LoadDatabaseConfig(j, cfg.database);
    LoadLibraryConfig(j, cfg.library);
    LoadImportConfig(j, cfg.import);
    LoadFiltersConfig(j, cfg.filters);
    LoadLoggingConfig(j, cfg.logging);

    return cfg;
}

CBookFilter::CBookFilter(const SFiltersConfig& cfg) : m_cfg(cfg) 
{}

SFilterResult CBookFilter::ShouldInclude(const Inpx::SBookRecord& rec) const
{
    // include whitelist
    if (!m_cfg.includeLanguages.empty()) 
    {
        bool found = std::find(m_cfg.includeLanguages.begin(),
                               m_cfg.includeLanguages.end(),
                               rec.language) != m_cfg.includeLanguages.end();
        if (!found) 
        { 
            return { false, "language not in include list: " + rec.language }; 
        }
    }
    // exclude list
    for (const auto& ex : m_cfg.excludeLanguages)
    {
        if (ex == rec.language) 
        { 
            return { false, "excluded language: " + rec.language }; 
        }
    }

    // genres
    if (!m_cfg.includeGenres.empty()) 
    {
        bool any = false;
        for (const auto& g : rec.genres)
        {
            if (std::find(m_cfg.includeGenres.begin(), m_cfg.includeGenres.end(), g)
                != m_cfg.includeGenres.end()) 
            { 
                any = true; break; 
            }
        }
        if (!any) 
        { 
            return { false, "genre not in include list" }; 
        }
    }
    for (const auto& g : rec.genres)
    {
        for (const auto& ex : m_cfg.excludeGenres)
        {
            if (ex == g) 
            { 
                return { false, "excluded genre: " + g }; 
            }
        }
    }

    // authors
    for (const auto& a : rec.authors)
    {
        std::string aName = a.lastName + " " + a.firstName;
        for (const auto& ex : m_cfg.excludeAuthors)
        {
            if (aName.find(ex) != std::string::npos) 
            { 
                return { false, "excluded author: " + ex }; 
            }
        }
    }

    // size
    if (m_cfg.minFileSize > 0 && rec.fileSize < m_cfg.minFileSize) 
    { 
        return { false, "file too small" }; 
    }
    if (m_cfg.maxFileSize > 0 && rec.fileSize > m_cfg.maxFileSize) 
    { 
        return { false, "file too large" }; 
    }

    // keywords
    if (!m_cfg.excludeKeywords.empty() && !rec.keywords.empty())
    {
        for (const auto& kw : m_cfg.excludeKeywords)
        {
            if (!kw.empty() && rec.keywords.find(kw) != std::string::npos)
            {
                return { false, "excluded keyword: " + kw };
            }
        }
    }

    return { true, "" };
}

std::filesystem::path CAppPaths::GetBookMetaDir(const std::filesystem::path& dbPath, int64_t id)
{
    return dbPath.parent_path() / "meta" / std::to_string(id);
}

} // namespace Librium::Config
