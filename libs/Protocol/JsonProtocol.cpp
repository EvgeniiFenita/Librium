#include "JsonProtocol.hpp"
#include "Service/AppService.hpp"
#include "Service/Request.hpp"
#include "Service/Response.hpp"
#include "Utils/Base64.hpp"
#include "Log/Logger.hpp"
#include <nlohmann/json.hpp>
#include <memory>

namespace Librium::Protocol {

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
        if (m_json.contains("params") && m_json["params"].contains(key) && m_json["params"][key].is_string())
        {
            return m_json["params"][key].get<std::string>();
        }
        return def;
    }

    int64_t GetInt(const std::string& key, int64_t def = 0) const override
    {
        if (m_json.contains("params") && m_json["params"].contains(key))
        {
            if (m_json["params"][key].is_number_integer()) 
                return m_json["params"][key].get<int64_t>();

            if (m_json["params"][key].is_string()) 
            {
                try 
                { 
                    return std::stoll(m_json["params"][key].get<std::string>()); 
                } 
                catch (const std::exception& e) 
                {
                    LOG_DEBUG("Failed to parse integer parameter '{}': {}", key, e.what());
                }
            }
        }
        return def;
    }

    bool GetBool(const std::string& key, bool def = false) const override
    {
        if (m_json.contains("params") && m_json["params"].contains(key) && m_json["params"][key].is_boolean())
        {
            return m_json["params"][key].get<bool>();
        }
        return def;
    }

    bool HasParam(const std::string& key) const override
    {
        return m_json.contains("params") && m_json["params"].contains(key);
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

        nlohmann::json progress_msg = {
            {"status", "progress"},
            {"data", {
                {"processed", processed},
                {"total", total}
            }}
        };
        m_send(Utils::CBase64::Encode(progress_msg.dump()));
    }

    void SetData(const Db::SImportStats& stats) override
    {
        m_data = {
            {"inserted", stats.booksInserted},
            {"updated", stats.booksUpdated},
            {"skipped", stats.booksSkipped},
            {"filtered", stats.booksFiltered},
            {"fb2_parsed", stats.fb2Parsed},
            {"fb2_errors", stats.fb2Errors},
            {"archives_processed", stats.archivesProcessed}
        };
    }

    void SetData(const Db::SQueryResult& result) override
    {
        nlohmann::json jsonResult = nlohmann::json::object();
        jsonResult["totalFound"] = result.totalFound;
        
        nlohmann::json booksArray = nlohmann::json::array();
        for (const auto& book : result.books)
        {
            nlohmann::json b = {
                {"id", book.id},
                {"title", book.title},
                {"language", book.language},
                {"ext", book.fileExt},
                {"size", book.fileSize},
                {"file", book.fileName},
                {"archive", book.archiveName}
            };

            if (!book.libId.empty()) b["libId"] = book.libId;
            if (book.rating > 0) b["rating"] = book.rating;
            if (!book.publisher.empty()) b["publisher"] = book.publisher;
            if (!book.isbn.empty()) b["isbn"] = book.isbn;
            if (!book.publishYear.empty()) b["publishYear"] = book.publishYear;
            if (!book.dateAdded.empty()) b["dateAdded"] = book.dateAdded;

            nlohmann::json authorsArray = nlohmann::json::array();
            for (const auto& a : book.authors)
            {
                nlohmann::json authorObj;
                if (!a.lastName.empty()) authorObj["lastName"] = a.lastName;
                if (!a.firstName.empty()) authorObj["firstName"] = a.firstName;
                if (!a.middleName.empty()) authorObj["middleName"] = a.middleName;
                authorsArray.push_back(authorObj);
            }
            b["authors"] = authorsArray;

            nlohmann::json genresArray = nlohmann::json::array();
            for (const auto& g : book.genres)
            {
                genresArray.push_back(g);
            }
            b["genres"] = genresArray;

            if (!book.series.empty())
            {
                b["series"] = book.series;
                if (book.seriesNumber > 0)
                {
                    b["seriesNumber"] = book.seriesNumber;
                }
            }

            if (!book.annotation.empty()) b["annotation"] = book.annotation;
            if (!book.keywords.empty()) b["keywords"] = book.keywords;

            booksArray.push_back(b);
        }
        jsonResult["books"] = booksArray;
        m_data = jsonResult;
    }

    void SetData(const Service::SAppStats& stats) override
    {
        m_data = {
            {"total_books", stats.totalBooks},
            {"total_authors", stats.totalAuthors}
        };
    }

    void SetData(const Service::SBookDetails& book) override
    {
        Db::SQueryResult temp;
        temp.books.push_back(book.book);
        temp.totalFound = 1;
        
        SetData(temp);
        
        if (!m_data.empty() && m_data.contains("books") && !m_data["books"].empty())
        {
            auto singleBook = m_data["books"][0];
            if (!book.coverPath.empty())
            {
                singleBook["cover"] = std::string(book.coverPath.begin(), book.coverPath.end());
            }
            m_data = singleBook;
        }
    }

    void SetDataExport(const std::filesystem::path& path, const std::string& filename) override
    {
        m_data = {
            {"file", path.u8string()},
            {"filename", filename}
        };
    }

    nlohmann::json ToJson() const
    {
        nlohmann::json j;
        j["status"] = m_status;
        if (m_status == "error")
        {
            j["error"] = m_error;
        }
        else if (!m_data.is_null())
        {
            j["data"] = m_data;
        }
        return j;
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
        LOG_DEBUG("INCOMING: {}", jsonStr);

        nlohmann::json parsed = nlohmann::json::parse(jsonStr);
        CJsonRequest req(parsed);
        CJsonResponse res(sendCallback);

        std::unique_ptr<Indexer::IProgressReporter> reporter;
        if (reporterFactory)
        {
            reporter = reporterFactory(res);
        }

        service.Dispatch(req, res, reporter.get());

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
    nlohmann::json err = {
        {"status", "error"},
        {"error", errorMsg}
    };
    return Utils::CBase64::Encode(err.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace));
}

} // namespace Librium::Protocol
