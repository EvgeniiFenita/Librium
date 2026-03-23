#include "Actions.hpp"
#include "Service/AppService.hpp"
#include "Service/LibraryApi.hpp"
#include "Config/AppConfig.hpp"
#include "Utils/StringUtils.hpp"

namespace Librium::Service {

// ----------------------------------------------------------------------------
// IMPORT
// ----------------------------------------------------------------------------
void CImportAction::Execute(CAppService& service, const IRequest& req, IResponse& res, Indexer::IProgressReporter* reporter)
{
    (void)req;
    auto stats = service.GetApi().Import(reporter);
    res.SetData(stats);
}

// ----------------------------------------------------------------------------
// UPGRADE
// ----------------------------------------------------------------------------
void CUpgradeAction::Execute(CAppService& service, const IRequest& req, IResponse& res, Indexer::IProgressReporter* reporter)
{
    (void)req;
    auto stats = service.GetApi().Upgrade(reporter);
    res.SetData(stats);
}

// ----------------------------------------------------------------------------
// QUERY
// ----------------------------------------------------------------------------
void CQueryAction::Execute(CAppService& service, const IRequest& req, IResponse& res, Indexer::IProgressReporter* reporter)
{
    (void)reporter;
    Db::SQueryParams qp;
    
    if (req.HasParam("title"))    qp.title = req.GetString("title");
    if (req.HasParam("author"))   qp.author = req.GetString("author");
    if (req.HasParam("genre"))    qp.genre = req.GetString("genre");
    if (req.HasParam("series"))   qp.series = req.GetString("series");
    if (req.HasParam("language")) qp.language = req.GetString("language");
    if (req.HasParam("libId"))    qp.libId = req.GetString("libId");
    if (req.HasParam("archiveName")) qp.archiveName = req.GetString("archiveName");
    if (req.HasParam("dateFrom")) qp.dateFrom = req.GetString("dateFrom");
    if (req.HasParam("dateTo"))   qp.dateTo = req.GetString("dateTo");
    
    if (req.HasParam("ratingMin")) qp.ratingMin = static_cast<int>(req.GetInt("ratingMin"));
    if (req.HasParam("ratingMax")) qp.ratingMax = static_cast<int>(req.GetInt("ratingMax"));
    if (req.HasParam("withAnnotation")) qp.withAnnotation = req.GetBool("withAnnotation");
    
    if (req.HasParam("limit"))    qp.limit = req.GetInt("limit");
    if (req.HasParam("offset"))   qp.offset = req.GetInt("offset");

    auto result = service.GetApi().SearchBooks(qp);
    res.SetData(result);
}

// ----------------------------------------------------------------------------
// EXPORT
// ----------------------------------------------------------------------------
void CExportAction::Execute(CAppService& service, const IRequest& req, IResponse& res, Indexer::IProgressReporter* reporter)
{
    (void)reporter;
    if (!req.HasParam("id"))
    {
        res.SetError("Missing book 'id'");
        return;
    }
    if (!req.HasParam("out"))
    {
        res.SetError("Missing 'out' directory");
        return;
    }

    try
    {
        int64_t id = req.GetInt("id");
        auto outDir = Config::Utf8ToPath(req.GetString("out"));
        
        auto bookOpt = service.GetApi().GetBook(id);
        if (!bookOpt)
        {
            res.SetError("Book not found");
            return;
        }

        std::string author;
        if (!bookOpt->book.authors.empty())
        {
            author = bookOpt->book.authors[0].lastName;
            if (!bookOpt->book.authors[0].firstName.empty())
            {
                author += " " + bookOpt->book.authors[0].firstName;
            }
        }
        if (author.empty()) author = "Unknown";
        
        std::string title = bookOpt->book.title.empty() ? "Unknown" : bookOpt->book.title;
        std::string filename = Utils::CStringUtils::SanitizeFilename(author + " - " + title + "." + bookOpt->book.fileExt);

        auto outPath = service.GetApi().ExportBook(id, outDir);
        res.SetDataExport(outPath, filename);
    }
    catch (const std::exception& e)
    {
        res.SetError(e.what());
    }
}

// ----------------------------------------------------------------------------
// STATS
// ----------------------------------------------------------------------------
void CStatsAction::Execute(CAppService& service, const IRequest& req, IResponse& res, Indexer::IProgressReporter* reporter)
{
    (void)req;
    (void)reporter;
    auto stats = service.GetApi().GetStats();
    res.SetData(stats);
}

// ----------------------------------------------------------------------------
// GET-BOOK
// ----------------------------------------------------------------------------
void CGetBookAction::Execute(CAppService& service, const IRequest& req, IResponse& res, Indexer::IProgressReporter* reporter)
{
    (void)reporter;
    if (!req.HasParam("id"))
    {
        res.SetError("Missing 'id' parameter");
        return;
    }

    auto bookOpt = service.GetApi().GetBook(req.GetInt("id"));
    if (!bookOpt)
    {
        res.SetError("Book not found");
        return;
    }

    res.SetData(*bookOpt);
}

} // namespace Librium::Service
