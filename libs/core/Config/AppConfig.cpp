#include "AppConfig.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <fstream>
#include <thread>

using json = nlohmann::json;

namespace LibIndexer::Config {

CAppConfig CAppConfig::Defaults()
{
    CAppConfig c;
    c.import.threadCount = static_cast<int>(
        std::max(1u, std::thread::hardware_concurrency()));
    return c;
}

CAppConfig CAppConfig::Load(const std::string& path)
{
    std::ifstream f(path);
    if (!f.is_open())
        throw CConfigError("Cannot open config: " + path);

    json j;
    try { j = json::parse(f); }
    catch (const json::parse_error& e)
    { throw CConfigError(std::string("JSON parse error: ") + e.what()); }

    CAppConfig cfg = Defaults();

    auto get = [&](const json& obj, const char* key, auto& val)
    {
        if (obj.contains(key)) val = obj[key].get<std::decay_t<decltype(val)>>();
    };

    if (j.contains("database")) get(j["database"], "path", cfg.database.path);
    if (j.contains("library"))
    {
        get(j["library"], "inpxPath",    cfg.library.inpxPath);
        get(j["library"], "archivesDir", cfg.library.archivesDir);
    }
    if (j.contains("import"))
    {
        get(j["import"], "parseFb2",            cfg.import.parseFb2);
        get(j["import"], "threadCount",         cfg.import.threadCount);
        get(j["import"], "transactionBatchSize",cfg.import.transactionBatchSize);
        get(j["import"], "mode",                cfg.import.mode);
    }
    if (j.contains("filters"))
    {
        auto& fi = j["filters"];
        auto gv = [&](const char* k, std::vector<std::string>& v)
        { if (fi.contains(k)) v = fi[k].get<std::vector<std::string>>(); };
        gv("excludeLanguages", cfg.filters.excludeLanguages);
        gv("includeLanguages", cfg.filters.includeLanguages);
        gv("excludeGenres",    cfg.filters.excludeGenres);
        gv("includeGenres",    cfg.filters.includeGenres);
        gv("excludeAuthors",   cfg.filters.excludeAuthors);
        gv("excludeKeywords",  cfg.filters.excludeKeywords);
        if (fi.contains("minFileSize")) cfg.filters.minFileSize = fi["minFileSize"].get<uint64_t>();
        if (fi.contains("maxFileSize")) cfg.filters.maxFileSize = fi["maxFileSize"].get<uint64_t>();
    }
    if (j.contains("logging"))
    {
        get(j["logging"], "level",            cfg.logging.level);
        get(j["logging"], "file",             cfg.logging.file);
        get(j["logging"], "progressInterval", cfg.logging.progressInterval);
    }
    return cfg;
}

void CAppConfig::Save(const std::string& path) const
{
    json j = {
        {"database", {{"path", database.path}}},
        {"library",  {{"inpxPath", library.inpxPath}, {"archivesDir", library.archivesDir}}},
        {"import",   {{"parseFb2", import.parseFb2}, {"threadCount", import.threadCount},
                      {"transactionBatchSize", import.transactionBatchSize}, {"mode", import.mode}}},
        {"filters",  {{"excludeLanguages", filters.excludeLanguages},
                      {"includeLanguages", filters.includeLanguages},
                      {"excludeGenres",    filters.excludeGenres},
                      {"includeGenres",    filters.includeGenres},
                      {"minFileSize",      filters.minFileSize},
                      {"maxFileSize",      filters.maxFileSize},
                      {"excludeAuthors",   filters.excludeAuthors},
                      {"excludeKeywords",  filters.excludeKeywords}}},
        {"logging",  {{"level", logging.level}, {"file", logging.file},
                      {"progressInterval", logging.progressInterval}}}
    };
    std::ofstream f(path);
    f << j.dump(2) << "\n";
}

CBookFilter::CBookFilter(const FiltersConfig& cfg) : m_cfg(cfg) {}

bool CBookFilter::ShouldInclude(const Inpx::BookRecord& rec) const
{
    m_lastReason.clear();

    // include whitelist
    if (!m_cfg.includeLanguages.empty())
    {
        bool found = std::find(m_cfg.includeLanguages.begin(),
                               m_cfg.includeLanguages.end(),
                               rec.language) != m_cfg.includeLanguages.end();
        if (!found) { m_lastReason = "language not in include list: " + rec.language; return false; }
    }
    // exclude list
    for (const auto& ex : m_cfg.excludeLanguages)
        if (ex == rec.language) { m_lastReason = "excluded language: " + rec.language; return false; }

    // genres
    if (!m_cfg.includeGenres.empty())
    {
        bool any = false;
        for (const auto& g : rec.genres)
            if (std::find(m_cfg.includeGenres.begin(), m_cfg.includeGenres.end(), g)
                != m_cfg.includeGenres.end()) { any = true; break; }
        if (!any) { m_lastReason = "genre not in include list"; return false; }
    }
    for (const auto& g : rec.genres)
        for (const auto& ex : m_cfg.excludeGenres)
            if (ex == g) { m_lastReason = "excluded genre: " + g; return false; }

    // size
    if (m_cfg.minFileSize > 0 && rec.fileSize < m_cfg.minFileSize)
    { m_lastReason = "file too small"; return false; }
    if (m_cfg.maxFileSize > 0 && rec.fileSize > m_cfg.maxFileSize)
    { m_lastReason = "file too large"; return false; }

    return true;
}

} // namespace LibIndexer::Config
