#include "Version.hpp"
#include "Indexer/Indexer.hpp"

#include "Config/AppConfig.hpp"
#include "Database/Database.hpp"
#include "Log/Logger.hpp"
#include "Query/BookQuery.hpp"
#include "Query/QuerySerializer.hpp"

#include <csignal>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include <CLI/CLI.hpp>
#include <sqlite3.h>

using namespace Librium;

static Indexer::CIndexer* g_indexer = nullptr;

static void OnSignal(int) 
{
    Log::CLogger::Instance().Warn("Interrupt received, stopping...");
    if (g_indexer) g_indexer->RequestStop();
}

static void SetupLogging(const Config::CLoggingConfig& cfg,
                          const std::string& overrideLevel = "") 
{
    using namespace Log;
    const std::string& lvl = overrideLevel.empty() ? cfg.level : overrideLevel;
    if      (lvl == "debug") CLogger::Instance().SetLevel(ELogLevel::Debug);
    else if (lvl == "warn")  CLogger::Instance().SetLevel(ELogLevel::Warn);
    else if (lvl == "error") CLogger::Instance().SetLevel(ELogLevel::Error);
    else                     CLogger::Instance().SetLevel(ELogLevel::Info);

    if (!cfg.file.empty())
        CLogger::Instance().SetFile(cfg.file);
}

static void PrintStats(const std::string& dbPath) 
{
    Db::CDatabase db(dbPath);
    sqlite3*      raw = db.Handle();

    auto count = [&](const char* sql) -> int64_t
    {
        sqlite3_stmt* s = nullptr;
        sqlite3_prepare_v2(raw, sql, -1, &s, nullptr);
        sqlite3_step(s);
        int64_t n = sqlite3_column_int64(s, 0);
        sqlite3_finalize(s);
        return n;
    };

    std::cout << "\n=== Database Statistics ===\n"
              << "  Books    : " << db.CountBooks()  << "\n"
              << "  Authors  : " << db.CountAuthors() << "\n"
              << "  Genres   : " << count("SELECT COUNT(*) FROM genres")     << "\n"
              << "  Series   : " << count("SELECT COUNT(*) FROM series")     << "\n"
              << "  Archives : " << count("SELECT COUNT(*) FROM archives WHERE last_indexed!=''") << "\n";

    auto printTop = [&](const char* title, const char* sql) 
{
        std::cout << "\n  " << title << ":\n";
        sqlite3_stmt* s = nullptr;
        sqlite3_prepare_v2(raw, sql, -1, &s, nullptr);
        while (sqlite3_step(s) == SQLITE_ROW) 
{
            const auto* name = reinterpret_cast<const char*>(sqlite3_column_text(s, 0));
            int64_t cnt      = sqlite3_column_int64(s, 1);
            std::cout << "    " << (name ? name : "(none)") << " : " << cnt << "\n";
        }
        sqlite3_finalize(s);
    };

    printTop("Top languages",
        "SELECT language, COUNT(*) cnt FROM books "
        "GROUP BY language ORDER BY cnt DESC LIMIT 10");

    printTop("Top genres",
        "SELECT g.code, COUNT(bg.book_id) cnt "
        "FROM genres g JOIN book_genres bg ON g.id=bg.genre_id "
        "GROUP BY g.id ORDER BY cnt DESC LIMIT 10");

    std::cout << "\n";
}

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>

std::string utf16_to_utf8(const std::wstring& utf16) 
{
    if (utf16.empty()) return {};
    auto u8 = std::filesystem::path(utf16).u8string();
    return std::string(u8.begin(), u8.end());
}

std::vector<std::string> get_utf8_args() 
{
    int argc;
    LPWSTR* argvw = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argvw) return {};
    std::vector<std::string> args;
    for (int i = 0; i < argc; ++i) args.push_back(utf16_to_utf8(argvw[i]));
    LocalFree(argvw);
    return args;
}
#endif

int main(int argc, char* argv[]) 
{
    std::vector<std::string> args;
#ifdef _WIN32
    args = get_utf8_args();
#else
    for (int i = 0; i < argc; ++i) args.push_back(argv[i]);
#endif

    CLI::App app{"Librium - INPX library indexer"};
    app.set_version_flag("--version,-v",
        std::string("Librium v") + Apps::Indexer::VersionString);
    
    // ... rest of main ...
    // Note: replacing argc/argv in CLI11_PARSE

    std::string logLevel;

    // ── import ────────────────────────────────────────────────────────────────
    auto* importCmd = app.add_subcommand("import", "Full import from INPX");
    std::string CImportConfig;
    int importThreads = 0;
    bool importNoFb2 = false;
    importCmd->add_option("--config", CImportConfig, "Config JSON path")->required();
    importCmd->add_option("--threads", importThreads, "Override thread count");
    importCmd->add_flag("--no-fb2", importNoFb2, "Skip FB2 parsing");
    importCmd->add_option("--log-level", logLevel, "Log level");

    // ── upgrade ───────────────────────────────────────────────────────────────
    auto* upgradeCmd = app.add_subcommand("upgrade", "Add only new archives");
    std::string upgradeConfig;
    int upgradeThreads = 0;
    bool upgradeNoFb2 = false;
    upgradeCmd->add_option("--config", upgradeConfig, "Config JSON path")->required();
    upgradeCmd->add_option("--threads", upgradeThreads, "Override thread count");
    upgradeCmd->add_flag("--no-fb2", upgradeNoFb2, "Skip FB2 parsing");
    upgradeCmd->add_option("--log-level", logLevel, "Log level");

    // ── stats ─────────────────────────────────────────────────────────────────
    auto* statsCmd = app.add_subcommand("stats", "Show database statistics");
    std::string statsDb;
    statsCmd->add_option("--db", statsDb, "Path to library.db")->required();

    // ── init-config ───────────────────────────────────────────────────────────
    auto* initCmd = app.add_subcommand("init-config", "Create default config file");
    std::string initOutput = "config.json";
    initCmd->add_option("--output", initOutput, "Output path");

    // Create argc/argv wrapper for CLI11
    std::vector<const char*> argv_utf8;
    for (const auto& a : args) argv_utf8.push_back(a.c_str());

    try {
        app.parse((int)argv_utf8.size(), argv_utf8.data());
    } catch (const CLI::ParseError &e) 
{
        return app.exit(e);
    }

    if (*initCmd) 
{
        Config::CAppConfig::Defaults().Save(initOutput);
        std::cout << "Config written to: " << initOutput << "\n";
        return 0;
    }

    if (*statsCmd) 
{
        try { PrintStats(statsDb); }
        catch (const std::exception& e) 
{ std::cerr << "Error: " << e.what() << "\n"; return 1; }
        return 0;
    }

    // import or upgrade
    bool isUpgrade = (bool)*upgradeCmd;
    const std::string& configPath = isUpgrade ? upgradeConfig : CImportConfig;
    int overrideThreads           = isUpgrade ? upgradeThreads : importThreads;
    bool noFb2                    = isUpgrade ? upgradeNoFb2   : importNoFb2;

    Config::CAppConfig cfg;
    try { cfg = Config::CAppConfig::Load(configPath); }
    catch (const std::exception& e) 
{ std::cerr << "Config error: " << e.what() << "\n"; return 1; }

    if (overrideThreads > 0) cfg.import.threadCount = overrideThreads;
    if (noFb2)               cfg.import.parseFb2 = false;
    if (isUpgrade)           cfg.import.mode = "upgrade";

    SetupLogging(cfg.logging, logLevel);
    std::signal(SIGINT, OnSignal);

    Log::CLogger::Instance().Info(
        "Librium v{} — mode: {}", Apps::Indexer::VersionString, cfg.import.mode);

    try
    {
        Indexer::CIndexer indexer(cfg);
        g_indexer = &indexer;
        (void)indexer.Run();
        g_indexer = nullptr;
    }
    catch (const std::exception& e) 
{
        Log::CLogger::Instance().Error("Fatal: {}", e.what());
        return 1;
    }

    return 0;
}






