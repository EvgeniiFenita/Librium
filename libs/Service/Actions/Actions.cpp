#include "Actions.hpp"
#include "Service/AppService.hpp"
#include "Service/LibraryApi.hpp"
#include "Config/AppConfig.hpp"
#include "Utils/StringUtils.hpp"

namespace Librium::Service {

namespace {

struct SQueryRequest
{
    std::string title;
    std::string author;
    std::string genre;
    std::string series;
    std::string language;
    std::string libId;
    std::string archiveName;
    std::string dateFrom;
    std::string dateTo;
    int ratingMin{0};
    int ratingMax{0};
    bool withAnnotation{false};
    int limit{0};
    int offset{0};
};

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

SQueryRequest ParseQueryRequest(const IRequest& req)
{
    SQueryRequest queryRequest;

    LoadStringParam(req, "title", queryRequest.title);
    LoadStringParam(req, "author", queryRequest.author);
    LoadStringParam(req, "genre", queryRequest.genre);
    LoadStringParam(req, "series", queryRequest.series);
    LoadStringParam(req, "language", queryRequest.language);
    LoadStringParam(req, "libId", queryRequest.libId);
    LoadStringParam(req, "archiveName", queryRequest.archiveName);
    LoadStringParam(req, "dateFrom", queryRequest.dateFrom);
    LoadStringParam(req, "dateTo", queryRequest.dateTo);

    LoadIntParam(req, "ratingMin", queryRequest.ratingMin);
    LoadIntParam(req, "ratingMax", queryRequest.ratingMax);
    LoadBoolParam(req, "withAnnotation", queryRequest.withAnnotation);
    LoadIntParam(req, "limit", queryRequest.limit);
    LoadIntParam(req, "offset", queryRequest.offset);

    return queryRequest;
}

Db::SQueryParams ToQueryParams(const SQueryRequest& request)
{
    Db::SQueryParams queryParams;
    queryParams.title = request.title;
    queryParams.author = request.author;
    queryParams.genre = request.genre;
    queryParams.series = request.series;
    queryParams.language = request.language;
    queryParams.libId = request.libId;
    queryParams.archiveName = request.archiveName;
    queryParams.dateFrom = request.dateFrom;
    queryParams.dateTo = request.dateTo;
    queryParams.ratingMin = request.ratingMin;
    queryParams.ratingMax = request.ratingMax;
    queryParams.withAnnotation = request.withAnnotation;
    queryParams.limit = request.limit;
    queryParams.offset = request.offset;
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
    const auto queryRequest = ParseQueryRequest(req);
    auto result = service.GetApi().SearchBooks(ToQueryParams(queryRequest));
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
