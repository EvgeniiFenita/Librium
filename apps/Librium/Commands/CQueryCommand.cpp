#include "CQueryCommand.hpp"
#include "Database/Database.hpp"
#include "Log/Logger.hpp"
#include "Query/BookQuery.hpp"
#include "Query/QuerySerializer.hpp"
#include "Utils.hpp"
#include <iostream>

namespace Librium::Apps {

void CQueryCommand::Setup(CLI::App& app) 
{
    auto* sub = app.add_subcommand("query", "Book query tool");
    sub->add_option("--db",     m_dbPath,     "Path to library.db")->required();
    sub->add_option("--output", m_outputPath, "Output JSON file")->required();

    sub->add_option("--title",       m_params.title,       "Partial title match");
    sub->add_option("--author",      m_params.author,      "Partial author name match");
    sub->add_option("--genre",       m_params.genre,       "Exact genre code");
    sub->add_option("--series",      m_params.series,      "Partial series match");
    sub->add_option("--language",    m_params.language,    "Exact language code");
    sub->add_option("--lib-id",      m_params.libId,       "Exact lib_id");
    sub->add_option("--archive",     m_params.archiveName, "Exact archive name");
    sub->add_option("--year-from",   m_params.yearFrom,    "Publish year >=");
    sub->add_option("--date-from",   m_params.dateFrom,    "Date added >= YYYY-MM-DD");
    sub->add_option("--date-to",     m_params.dateTo,      "Date added <= YYYY-MM-DD");
    sub->add_option("--rating-min",  m_params.ratingMin,   "Rating >=");
    sub->add_option("--limit",       m_params.limit,       "Max results (0=unlimited, default 100)");
    sub->add_option("--offset",      m_params.offset,      "Pagination offset");
    sub->add_flag("--with-annotation", m_params.withAnnotation, "Only books with annotation");
    sub->add_flag("--with-cover",      m_params.withCover,      "Only books with cover");

    sub->add_option("--log-level", m_logLevel, "debug/info/warn/error");
}

int CQueryCommand::Execute() 
{
    if (!m_logLevel.empty()) 
    {
        SetupLogging(ParseLogLevel(m_logLevel));
    }

    try
    {
        Db::CDatabase db(m_dbPath);
        const auto result = Query::CBookQuery::Execute(db, m_params);

        Log::CLogger::Instance().Info(
            "Found {} books (total matching: {})",
            result.books.size(), result.totalFound);

        Query::CQuerySerializer::SaveToFile(result, m_outputPath);
        Log::CLogger::Instance().Info("Saved to {}", m_outputPath);
    }
    catch (const std::exception& e) 
    {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}

} // namespace Librium::Apps
