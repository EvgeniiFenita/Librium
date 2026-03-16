#include "Version.hpp"

#include "Db/Database.hpp"
#include "Log/Logger.hpp"
#include "Query/BookQuery.hpp"
#include "Query/QuerySerializer.hpp"

#include <iostream>
#include <string>

#include <CLI/CLI.hpp>

using namespace LibIndexer;

int main(int argc, char* argv[])
{
    CLI::App app{"libquery — book query tool for libindexer database"};
    app.set_version_flag("--version,-v",
        std::string("libquery v") + Apps::Query::VersionString);

    std::string dbPath;
    std::string outputPath;
    app.add_option("--db",     dbPath,     "Path to library.db")->required();
    app.add_option("--output", outputPath, "Output JSON file")->required();

    Query::QueryParams params;
    app.add_option("--title",       params.title,       "Partial title match");
    app.add_option("--author",      params.author,      "Partial author name match");
    app.add_option("--genre",       params.genre,       "Exact genre code");
    app.add_option("--series",      params.series,      "Partial series match");
    app.add_option("--language",    params.language,    "Exact language code");
    app.add_option("--lib-id",      params.libId,       "Exact lib_id");
    app.add_option("--archive",     params.archiveName, "Exact archive name");
    app.add_option("--year-from",   params.yearFrom,    "Publish year >=");
    app.add_option("--date-from",   params.dateFrom,    "Date added >= YYYY-MM-DD");
    app.add_option("--date-to",     params.dateTo,      "Date added <= YYYY-MM-DD");
    app.add_option("--rating-min",  params.ratingMin,   "Rating >=");
    app.add_option("--limit",       params.limit,       "Max results (0=unlimited, default 100)");
    app.add_option("--offset",      params.offset,      "Pagination offset");
    app.add_flag("--with-annotation", params.withAnnotation, "Only books with annotation");
    app.add_flag("--with-cover",      params.withCover,      "Only books with cover");

    std::string logLevel;
    app.add_option("--log-level", logLevel, "debug/info/warn/error");

    CLI11_PARSE(app, argc, argv);

    if (!logLevel.empty())
    {
        using namespace Log;
        if      (logLevel == "debug") CLogger::Instance().SetLevel(ELogLevel::Debug);
        else if (logLevel == "warn")  CLogger::Instance().SetLevel(ELogLevel::Warn);
        else if (logLevel == "error") CLogger::Instance().SetLevel(ELogLevel::Error);
        else                          CLogger::Instance().SetLevel(ELogLevel::Info);
    }

    try
    {
        Db::CDatabase db(dbPath);
        const auto result = Query::CBookQuery::Execute(db, params);

        Log::CLogger::Instance().Info(
            "Found {} books (total matching: {})",
            result.books.size(), result.totalFound);

        Query::CQuerySerializer::SaveToFile(result, outputPath);

        Log::CLogger::Instance().Info("Saved to {}", outputPath);
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
