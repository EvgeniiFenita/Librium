#include "CHelperCommands.hpp"
#include "Database/Database.hpp"
#include "Config/AppConfig.hpp"
#include <iostream>
#include <sqlite3.h>

namespace Librium::Apps {

void CStatsCommand::Setup(CLI::App& app) 
{
    auto* sub = app.add_subcommand("stats", "Show database statistics");
    sub->add_option("--db", m_dbPath, "Path to library.db")->required();
}

int CStatsCommand::Execute() 
{
    try 
    {
        Db::CDatabase db(m_dbPath);
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
    catch (const std::exception& e) 
    { 
        std::cerr << "Error: " << e.what() << "\n"; 
        return 1; 
    }
    return 0;
}

void CInitConfigCommand::Setup(CLI::App& app) 
{
    auto* sub = app.add_subcommand("init-config", "Create default config file");
    sub->add_option("--output", m_outputPath, "Output path");
}

int CInitConfigCommand::Execute() 
{
    Config::CAppConfig::Defaults().Save(m_outputPath);
    std::cout << "Config written to: " << m_outputPath << "\n";
    return 0;
}

} // namespace Librium::Apps
