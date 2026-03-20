#include "Actions.hpp"
#include "Service/AppService.hpp"
#include "Service/LibraryApi.hpp"
#include "Query/QuerySerializer.hpp"
#include "Config/AppConfig.hpp"

namespace Librium::Service {

// ----------------------------------------------------------------------------
// IMPORT
// ----------------------------------------------------------------------------
nlohmann::json CImportAction::Execute(CAppService& service, const nlohmann::json& params, Indexer::IProgressReporter* reporter)
{
    (void)params;
    auto stats = service.GetApi().Import(reporter);

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
    auto stats = service.GetApi().Upgrade(reporter);

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
    Query::SQueryParams qp;
    if (params.contains("title"))    qp.title = params["title"];
    if (params.contains("author"))   qp.author = params["author"];
    if (params.contains("genre"))    qp.genre = params["genre"];
    if (params.contains("series"))   qp.series = params["series"];
    if (params.contains("language")) qp.language = params["language"];
    if (params.contains("limit"))    qp.limit = params["limit"];
    if (params.contains("offset"))   qp.offset = params["offset"];

    auto result = service.GetApi().SearchBooks(qp);
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

    try
    {
        auto outDir = Config::Utf8ToPath(params["out"]);
        auto outPath = service.GetApi().ExportBook(params["id"], outDir);
        return {{"status", "ok"}, {"data", {{"file", outPath.u8string()}}}};
    }
    catch (const std::exception& e)
    {
        return {{"status", "error"}, {"error", e.what()}};
    }
}

// ----------------------------------------------------------------------------
// STATS
// ----------------------------------------------------------------------------
nlohmann::json CStatsAction::Execute(CAppService& service, const nlohmann::json& params, Indexer::IProgressReporter* reporter)
{
    (void)params;
    (void)reporter;
    auto stats = service.GetApi().GetStats();
    
    return {{"status", "ok"}, {"data", {
        {"total_books", stats.totalBooks},
        {"total_authors", stats.totalAuthors}
    }}};
}

// ----------------------------------------------------------------------------
// GET-BOOK
// ----------------------------------------------------------------------------
nlohmann::json CGetBookAction::Execute(CAppService& service, const nlohmann::json& params, Indexer::IProgressReporter* reporter)
{
    (void)reporter;
    if (!params.contains("id")) return {{"status", "error"}, {"error", "Missing 'id' parameter"}};

    auto bookOpt = service.GetApi().GetBook(params["id"]);
    if (!bookOpt) return {{"status", "error"}, {"error", "Book not found"}};

    // We can use QuerySerializer to reuse the logic of converting SBookResult to JSON
    // But ToJson takes SQueryResult. Let's create a temporary result.
    Query::SQueryResult result;
    result.books.push_back(std::move(bookOpt->book));
    result.totalFound = 1;

    auto json = Query::CQuerySerializer::ToJson(result);
    auto bookData = json["books"][0];

    if (!bookOpt->coverPath.empty())
    {
        bookData["cover"] = bookOpt->coverPath;
    }

    return {{"status", "ok"}, {"data", bookData}};
}

} // namespace Librium::Service
