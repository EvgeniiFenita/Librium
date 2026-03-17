#include "QuerySerializer.hpp"

#include <nlohmann/json.hpp>
#include <fstream>

namespace Librium::Query {

nlohmann::json CQuerySerializer::ToJson(const SQueryResult& result)
{
    nlohmann::json j;
    j["totalFound"] = result.totalFound;
    j["books"] = nlohmann::json::array();

    for (const auto& b : result.books)
    {
        nlohmann::json jb;
        jb["id"] = b.id;
        jb["libId"] = b.libId;
        jb["title"] = b.title;
        jb["series"] = b.series;
        jb["seriesNumber"] = b.seriesNumber;
        jb["language"] = b.language;
        jb["dateAdded"] = b.dateAdded;
        jb["rating"] = b.rating;
        jb["fileSize"] = b.fileSize;
        jb["archiveName"] = b.archiveName;
        jb["annotation"] = b.annotation;
        jb["publisher"] = b.publisher;
        jb["isbn"] = b.isbn;
        jb["publishYear"] = b.publishYear;

        jb["authors"] = nlohmann::json::array();
        for (const auto& a : b.authors)
        {
            jb["authors"].push_back({
                {"lastName", a.lastName},
                {"firstName", a.firstName},
                {"middleName", a.middleName}
            });
        }

        jb["genres"] = b.genres;
        j["books"].push_back(jb);
    }

    const auto& p = result.params;
    j["params"] = {
        {"title",          p.title},
        {"author",         p.author},
        {"genre",          p.genre},
        {"series",         p.series},
        {"language",       p.language},
        {"libId",          p.libId},
        {"archiveName",    p.archiveName},
        {"dateFrom",       p.dateFrom},
        {"dateTo",         p.dateTo},
        {"ratingMin",      p.ratingMin},
        {"withAnnotation", p.withAnnotation},
        {"limit",          p.limit},
        {"offset",         p.offset}
    };

    return j;
}

void CQuerySerializer::SaveToFile(const SQueryResult& result, const std::string& path)
{
    std::ofstream f(path);
    f << ToJson(result).dump(2) << "\n";
}

} // namespace Librium::Query
