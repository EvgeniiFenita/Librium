#include "Actions.hpp"
#include "Service/AppService.hpp"
#include "Service/LibraryApi.hpp"
#include "Config/AppConfig.hpp"
#include "Utils/StringUtils.hpp"

namespace Librium::Service {

namespace {

bool RequireParam(const IRequest& req, IResponse& res, const char* key, const std::string& error)
{
    if (req.HasParam(key))
    {
        return true;
    }

    res.SetError(error);
    return false;
}

void LoadStringParam(const IRequest& req, const char* key, std::string& target)
{
    if (req.HasParam(key))
    {
        target = req.GetString(key);
    }
}

template<typename TValue>
void LoadIntParam(const IRequest& req, const char* key, TValue& target)
{
    if (req.HasParam(key))
    {
        target = static_cast<TValue>(req.GetInt(key));
    }
}

void LoadBoolParam(const IRequest& req, const char* key, bool& target)
{
    if (req.HasParam(key))
    {
        target = req.GetBool(key);
    }
}

Db::SQueryParams BuildQueryParams(const IRequest& req)
{
    Db::SQueryParams queryParams;

    LoadStringParam(req, "title", queryParams.title);
    LoadStringParam(req, "author", queryParams.author);
    LoadStringParam(req, "genre", queryParams.genre);
    LoadStringParam(req, "series", queryParams.series);
    LoadStringParam(req, "language", queryParams.language);
    LoadStringParam(req, "libId", queryParams.libId);
    LoadStringParam(req, "archiveName", queryParams.archiveName);
    LoadStringParam(req, "dateFrom", queryParams.dateFrom);
    LoadStringParam(req, "dateTo", queryParams.dateTo);

    LoadIntParam(req, "ratingMin", queryParams.ratingMin);
    LoadIntParam(req, "ratingMax", queryParams.ratingMax);
    LoadBoolParam(req, "withAnnotation", queryParams.withAnnotation);
    LoadIntParam(req, "limit", queryParams.limit);
    LoadIntParam(req, "offset", queryParams.offset);

    return queryParams;
}

std::string BuildExportFilename(const Service::SBookDetails& book)
{
    std::string author = "Unknown";
    if (!book.book.authors.empty())
    {
        author = book.book.authors[0].lastName;
        if (!book.book.authors[0].firstName.empty())
        {
            author += " " + book.book.authors[0].firstName;
        }
        if (author.empty())
        {
            author = "Unknown";
        }
    }

    const std::string title = book.book.title.empty() ? "Unknown" : book.book.title;
    return Utils::CStringUtils::SanitizeFilename(author + " - " + title + "." + book.book.fileExt);
}

} // namespace

void CImportAction::Execute(CAppService& service, const IRequest& req, IResponse& res, const std::shared_ptr<Indexer::IProgressReporter>& reporter)
{
    (void)req;
    auto stats = service.GetApi().Import(reporter);
    res.SetData(stats);
}

void CUpgradeAction::Execute(CAppService& service, const IRequest& req, IResponse& res, const std::shared_ptr<Indexer::IProgressReporter>& reporter)
{
    (void)req;
    auto stats = service.GetApi().Upgrade(reporter);
    res.SetData(stats);
}

void CQueryAction::Execute(CAppService& service, const IRequest& req, IResponse& res, const std::shared_ptr<Indexer::IProgressReporter>& reporter)
{
    (void)reporter;
    auto result = service.GetApi().SearchBooks(BuildQueryParams(req));
    res.SetData(result);
}

void CExportAction::Execute(CAppService& service, const IRequest& req, IResponse& res, const std::shared_ptr<Indexer::IProgressReporter>& reporter)
{
    (void)reporter;
    if (!RequireParam(req, res, "id", "Missing book 'id'") ||
        !RequireParam(req, res, "out", "Missing 'out' directory"))
    {
        return;
    }

    try
    {
        int64_t id = req.GetInt("id");
        auto outDir = Utils::CStringUtils::Utf8ToPath(req.GetString("out"));
        
        auto bookOpt = service.GetApi().GetBook(id);
        if (!bookOpt)
        {
            res.SetError("Book not found");
            return;
        }

        auto outPath = service.GetApi().ExportBook(id, outDir);
        res.SetDataExport(outPath, BuildExportFilename(*bookOpt));
    }
    catch (const std::exception& e)
    {
        res.SetError(e.what());
    }
}

void CStatsAction::Execute(CAppService& service, const IRequest& req, IResponse& res, const std::shared_ptr<Indexer::IProgressReporter>& reporter)
{
    (void)req;
    (void)reporter;
    auto stats = service.GetApi().GetStats();
    res.SetData(stats);
}

void CGetBookAction::Execute(CAppService& service, const IRequest& req, IResponse& res, const std::shared_ptr<Indexer::IProgressReporter>& reporter)
{
    (void)reporter;
    if (!RequireParam(req, res, "id", "Missing 'id' parameter"))
    {
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
