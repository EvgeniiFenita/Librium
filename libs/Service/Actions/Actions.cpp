#include "Actions.hpp"
#include "Service/AppService.hpp"
#include "Indexer/Indexer.hpp"
#include "Query/BookQuery.hpp"
#include "Query/QuerySerializer.hpp"
#include "Log/Logger.hpp"
#include "Zip/ZipReader.hpp"
#include <filesystem>
#include <fstream>

namespace Librium::Service {

// ----------------------------------------------------------------------------
// IMPORT
// ----------------------------------------------------------------------------
nlohmann::json CImportAction::Execute(CAppService& service, const nlohmann::json& params, Indexer::IProgressReporter* reporter)
{
    (void)params;
    auto cfg = service.GetConfig();
    cfg.import.mode = "full";
    
    Indexer::CIndexer indexer(cfg);
    auto stats = indexer.Run(reporter);

    return {{"status", "ok"}, {"data", {
        {"inserted", stats.booksInserted},
        {"updated", stats.booksUpdated},
        {"skipped", stats.booksSkipped},
        {"filtered", stats.booksFiltered},
        {"fb2_parsed", stats.fb2Parsed},
        {"fb2_errors", stats.fb2Errors},
        {"archives_processed", stats.archivesProcessed}
    }}};
}

// ----------------------------------------------------------------------------
// UPGRADE
// ----------------------------------------------------------------------------
nlohmann::json CUpgradeAction::Execute(CAppService& service, const nlohmann::json& params, Indexer::IProgressReporter* reporter)
{
    (void)params;
    auto cfg = service.GetConfig();
    cfg.import.mode = "upgrade";
    
    Indexer::CIndexer indexer(cfg);
    auto stats = indexer.Run(reporter);

    return {{"status", "ok"}, {"data", {
        {"inserted", stats.booksInserted},
        {"updated", stats.booksUpdated},
        {"skipped", stats.booksSkipped},
        {"filtered", stats.booksFiltered},
        {"fb2_parsed", stats.fb2Parsed},
        {"fb2_errors", stats.fb2Errors},
        {"archives_processed", stats.archivesProcessed}
    }}};
}

// ----------------------------------------------------------------------------
// QUERY
// ----------------------------------------------------------------------------
nlohmann::json CQueryAction::Execute(CAppService& service, const nlohmann::json& params, Indexer::IProgressReporter* reporter)
{
    (void)reporter;
    auto& db = service.GetDatabase();
    
    Query::SQueryParams qp;
    if (params.contains("title"))    qp.title = params["title"];
    if (params.contains("author"))   qp.author = params["author"];
    if (params.contains("genre"))    qp.genre = params["genre"];
    if (params.contains("series"))   qp.series = params["series"];
    if (params.contains("language")) qp.language = params["language"];
    if (params.contains("limit"))    qp.limit = params["limit"];
    if (params.contains("offset"))   qp.offset = params["offset"];

    auto result = Query::CBookQuery::Execute(db, qp);
    return {{"status", "ok"}, {"data", Query::CQuerySerializer::ToJson(result)}};
}

// ----------------------------------------------------------------------------
// EXPORT
// ----------------------------------------------------------------------------
nlohmann::json CExportAction::Execute(CAppService& service, const nlohmann::json& params, Indexer::IProgressReporter* reporter)
{
    (void)reporter;
    if (!params.contains("id"))  return {{"status", "error"}, {"error", "Missing book 'id'"}};
    if (!params.contains("out")) return {{"status", "error"}, {"error", "Missing 'out' directory"}};

    auto& db = service.GetDatabase();
    auto bookInfo = db.GetBookPath(params["id"]);
    if (!bookInfo) return {{"status", "error"}, {"error", "Book not found"}};

    std::string zipName = bookInfo->archiveName;
    if (zipName.size() < 4 || zipName.substr(zipName.size() - 4) != ".zip") zipName += ".zip";

    auto zipPath = Config::Utf8ToPath(service.GetConfig().library.archivesDir) / Config::Utf8ToPath(zipName);
    auto data = Zip::CZipReader::ReadEntry(zipPath, bookInfo->fileName);
    
    auto outDir = Config::Utf8ToPath(params["out"]);
    if (!std::filesystem::exists(outDir)) std::filesystem::create_directories(outDir);

    auto outPath = outDir / Config::Utf8ToPath(bookInfo->fileName);
    std::ofstream ofs(outPath, std::ios::binary);
    ofs.write(reinterpret_cast<const char*>(data.data()), data.size());

    return {{"status", "ok"}, {"data", {{"file", outPath.u8string()}}}};
}

// ----------------------------------------------------------------------------
// STATS
// ----------------------------------------------------------------------------
nlohmann::json CStatsAction::Execute(CAppService& service, const nlohmann::json& params, Indexer::IProgressReporter* reporter)
{
    (void)params;
    (void)reporter;
    auto& db = service.GetDatabase();
    
    return {{"status", "ok"}, {"data", {
        {"total_books", db.CountBooks()},
        {"total_authors", db.CountAuthors()}
    }}};
}

// ----------------------------------------------------------------------------
// GET-BOOK
// ----------------------------------------------------------------------------
nlohmann::json CGetBookAction::Execute(CAppService& service, const nlohmann::json& params, Indexer::IProgressReporter* reporter)
{
    (void)reporter;
    if (!params.contains("id")) return {{"status", "error"}, {"error", "Missing 'id' parameter"}};

    int64_t id = params["id"];
    auto& db = service.GetDatabase();

    auto book = Query::CBookQuery::GetBookById(db, id);
    if (!book) return {{"status", "error"}, {"error", "Book not found"}};

    // We can use QuerySerializer to reuse the logic of converting SBookResult to JSON
    // But ToJson takes SQueryResult. Let's create a temporary result.
    Query::SQueryResult result;
    result.books.push_back(std::move(*book));
    result.totalFound = 1;

    auto json = Query::CQuerySerializer::ToJson(result);
    auto bookData = json["books"][0];

    // Check for cover
    try
    {
        auto metaDir = Config::GetBookMetaDir(Config::Utf8ToPath(service.GetConfig().database.path), id);
        
        if (std::filesystem::exists(metaDir) && std::filesystem::is_directory(metaDir))
        {
            for (const auto& entry : std::filesystem::directory_iterator(metaDir))
            {
                if (entry.is_regular_file())
                {
                    auto filename = entry.path().filename().u8string();
                    if (filename.find(u8"cover.") == 0)
                    {
                        bookData["cover"] = entry.path().u8string();
                        break;
                    }
                }
            }
        }
    }
    catch (const std::exception& e)
    {
        LOG_WARN("Failed to check cover for book {}: {}", id, e.what());
    }

    return {{"status", "ok"}, {"data", bookData}};
}

} // namespace Librium::Service
