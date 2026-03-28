#include "JsonProtocol.hpp"
#include "Service/AppService.hpp"
#include "Service/Request.hpp"
#include "Service/Response.hpp"
#include "Utils/Base64.hpp"
#include "Log/Logger.hpp"

#include <nlohmann/json.hpp>

#include <memory>

namespace Librium::Protocol {

namespace {

const nlohmann::json& GetParamsObject(const nlohmann::json& json)
{
    static const nlohmann::json kEmptyObject = nlohmann::json::object();
    if (json.contains("params") && json["params"].is_object())
    {
        return json["params"];
    }
    return kEmptyObject;
}

bool HasTypedParam(const nlohmann::json& json, const std::string& key)
{
    const auto& params = GetParamsObject(json);
    return params.contains(key);
}

const nlohmann::json* FindParam(const nlohmann::json& json, const std::string& key)
{
    const auto& params = GetParamsObject(json);
    const auto it = params.find(key);
    if (it == params.end())
    {
        return nullptr;
    }

    return &(*it);
}

std::string ReadStringParam(const nlohmann::json& json, const std::string& key, const std::string& def)
{
    const auto* value = FindParam(json, key);
    if (value && value->is_string())
    {
        return value->get<std::string>();
    }

    return def;
}

int64_t ReadIntParam(const nlohmann::json& json, const std::string& key, int64_t def)
{
    const auto* value = FindParam(json, key);
    if (!value)
    {
        return def;
    }

    if (value->is_number_integer())
    {
        return value->get<int64_t>();
    }

    if (value->is_string())
    {
        try
        {
            return std::stoll(value->get<std::string>());
        }
        catch (const std::exception& e)
        {
            LOG_DEBUG("Failed to parse integer parameter '{}': {}", key, e.what());
        }
    }

    return def;
}

bool ReadBoolParam(const nlohmann::json& json, const std::string& key, bool def)
{
    const auto* value = FindParam(json, key);
    if (value && value->is_boolean())
    {
        return value->get<bool>();
    }

    return def;
}

nlohmann::json BuildProgressJson(size_t processed, size_t total)
{
    return {
        {"status", "progress"},
        {"data", {
            {"processed", processed},
            {"total", total}
        }}
    };
}

nlohmann::json BuildImportStatsJson(const Db::SImportStats& stats)
{
    return {
        {"inserted", stats.booksInserted},
        {"updated", stats.booksUpdated},
        {"skipped", stats.booksSkipped},
        {"filtered", stats.booksFiltered},
        {"fb2_parsed", stats.fb2Parsed},
        {"fb2_errors", stats.fb2Errors},
        {"archives_processed", stats.archivesProcessed}
    };
}

nlohmann::json BuildAuthorJson(const Db::SAuthorInfo& author)
{
    nlohmann::json jsonAuthor = nlohmann::json::object();
    if (!author.lastName.empty()) jsonAuthor["lastName"] = author.lastName;
    if (!author.firstName.empty()) jsonAuthor["firstName"] = author.firstName;
    if (!author.middleName.empty()) jsonAuthor["middleName"] = author.middleName;
    return jsonAuthor;
}

nlohmann::json BuildBookJson(const Db::SBookResult& book)
{
    nlohmann::json jsonBook = {
        {"id", book.id},
        {"title", book.title},
        {"language", book.language},
        {"ext", book.fileExt},
        {"size", book.fileSize},
        {"file", book.fileName},
        {"archive", book.archiveName}
    };

    if (!book.libId.empty()) jsonBook["libId"] = book.libId;
    if (book.rating > 0) jsonBook["rating"] = book.rating;
    if (!book.publisher.empty()) jsonBook["publisher"] = book.publisher;
    if (!book.isbn.empty()) jsonBook["isbn"] = book.isbn;
    if (!book.publishYear.empty()) jsonBook["publishYear"] = book.publishYear;
    if (!book.dateAdded.empty()) jsonBook["dateAdded"] = book.dateAdded;

    nlohmann::json authors = nlohmann::json::array();
    for (const auto& author : book.authors)
    {
        authors.push_back(BuildAuthorJson(author));
    }
    jsonBook["authors"] = std::move(authors);

    nlohmann::json genres = nlohmann::json::array();
    for (const auto& genre : book.genres)
    {
        genres.push_back(genre);
    }
    jsonBook["genres"] = std::move(genres);

    if (!book.series.empty())
    {
        jsonBook["series"] = book.series;
        if (book.seriesNumber > 0)
        {
            jsonBook["seriesNumber"] = book.seriesNumber;
        }
    }

    if (!book.annotation.empty()) jsonBook["annotation"] = book.annotation;
    if (!book.keywords.empty()) jsonBook["keywords"] = book.keywords;

    return jsonBook;
}

nlohmann::json BuildQueryResultJson(const Db::SQueryResult& result)
{
    nlohmann::json books = nlohmann::json::array();
    for (const auto& book : result.books)
    {
        books.push_back(BuildBookJson(book));
    }

    return {
        {"totalFound", result.totalFound},
        {"books", std::move(books)}
    };
}

nlohmann::json BuildAppStatsJson(const Service::SAppStats& stats)
{
    return {
        {"total_books", stats.totalBooks},
        {"total_authors", stats.totalAuthors},
        {"indexed_archives", stats.indexedArchives}
    };
}

nlohmann::json BuildBookDetailsJson(const Service::SBookDetails& details)
{
    nlohmann::json jsonBook = BuildBookJson(details.book);
    if (!details.coverPath.empty())
    {
        jsonBook["cover"] = std::string(details.coverPath.begin(), details.coverPath.end());
    }
    return jsonBook;
}

nlohmann::json BuildExportJson(const std::filesystem::path& path, const std::string& filename)
{
    return {
        {"file", path.u8string()},
        {"filename", filename}
    };
}

nlohmann::json BuildEnvelope(std::string_view status, const nlohmann::json& data, const std::string& error)
{
    nlohmann::json json;
    json["status"] = status;
    if (status == "error")
    {
        json["error"] = error;
    }
    else if (!data.is_null())
    {
        json["data"] = data;
    }
    return json;
}

} // namespace

class CJsonRequest : public Service::IRequest
{
public:
    explicit CJsonRequest(const nlohmann::json& json) : m_json(json) {}

    std::string GetAction() const override
    {
        return m_json.value("action", "");
    }

    std::string GetString(const std::string& key, const std::string& def = "") const override
    {
        return ReadStringParam(m_json, key, def);
    }

    int64_t GetInt(const std::string& key, int64_t def = 0) const override
    {
        return ReadIntParam(m_json, key, def);
    }

    bool GetBool(const std::string& key, bool def = false) const override
    {
        return ReadBoolParam(m_json, key, def);
    }

    bool HasParam(const std::string& key) const override
    {
        return HasTypedParam(m_json, key);
    }

private:
    nlohmann::json m_json;
};

class CJsonResponse : public Service::IResponse
{
public:
    explicit CJsonResponse(CJsonProtocol::SendCallback send) 
        : m_status("ok"), m_send(std::move(send)) {}

    void SetError(const std::string& message) override
    {
        m_status = "error";
        m_error = message;
    }

    void SetProgress(size_t processed, size_t total) override
    {
        if (!m_send) return;
        m_send(Utils::CBase64::Encode(BuildProgressJson(processed, total).dump()));
    }

    void SetData(const Db::SImportStats& stats) override
    {
        m_data = BuildImportStatsJson(stats);
    }

    void SetData(const Db::SQueryResult& result) override
    {
        m_data = BuildQueryResultJson(result);
    }

    void SetData(const Service::SAppStats& stats) override
    {
        m_data = BuildAppStatsJson(stats);
    }

    void SetData(const Service::SBookDetails& book) override
    {
        m_data = BuildBookDetailsJson(book);
    }

    void SetDataExport(const std::filesystem::path& path, const std::string& filename) override
    {
        m_data = BuildExportJson(path, filename);
    }

    nlohmann::json ToJson() const
    {
        return BuildEnvelope(m_status, m_data, m_error);
    }

private:
    std::string m_status;
    std::string m_error;
    nlohmann::json m_data;
    CJsonProtocol::SendCallback m_send;
};

std::string CJsonProtocol::Process(
    const std::string& base64Request, 
    Service::CAppService& service, 
    SendCallback sendCallback,
    ReporterFactory reporterFactory)
{
    try
    {
        std::string jsonStr = Utils::CBase64::Decode(base64Request);
        LOG_INFO("INCOMING: {}", jsonStr);

        nlohmann::json parsed = nlohmann::json::parse(jsonStr);
        CJsonRequest req(parsed);
        CJsonResponse res(sendCallback);

        std::shared_ptr<Indexer::IProgressReporter> reporter;
        if (reporterFactory)
        {
            reporter = reporterFactory(res);
        }

        service.Dispatch(req, res, reporter);

        std::string responseStr = res.ToJson().dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
        LOG_DEBUG("OUTGOING: {}", responseStr);

        return Utils::CBase64::Encode(responseStr);
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("Protocol parsing error: {}", e.what());
        return BuildErrorResponse(std::string("Protocol parsing error: ") + e.what());
    }
}

std::string CJsonProtocol::BuildErrorResponse(const std::string& errorMsg)
{
    return Utils::CBase64::Encode(
        BuildEnvelope("error", nlohmann::json{}, errorMsg)
            .dump(-1, ' ', false, nlohmann::json::error_handler_t::replace));
}

} // namespace Librium::Protocol
