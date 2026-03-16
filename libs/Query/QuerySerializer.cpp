#include "QuerySerializer.hpp"

#include <fstream>

namespace Librium::Query {

nlohmann::json CQuerySerializer::ToJson(const QueryResult& result) 
{
    nlohmann::json jBooks = nlohmann::json::array();
    for (const auto& b : result.books) 
{
        nlohmann::json jAuthors = nlohmann::json::array();
        for (const auto& a : b.authors)
            jAuthors.push_back({
                {"lastName",   a.lastName},
                {"firstName",  a.firstName},
                {"middleName", a.middleName}
            });

        nlohmann::json jGenres = nlohmann::json::array();
        for (const auto& g : b.genres)
            jGenres.push_back(g);

        jBooks.push_back({
            {"libId",        b.libId},
            {"title",        b.title},
            {"authors",      jAuthors},
            {"genres",       jGenres},
            {"series",       b.series},
            {"seriesNumber", b.seriesNumber},
            {"language",     b.language},
            {"dateAdded",    b.dateAdded},
            {"rating",       b.rating},
            {"fileSize",     b.fileSize},
            {"archiveName",  b.archiveName},
            {"annotation",   b.annotation},
            {"publisher",    b.publisher},
            {"isbn",         b.isbn},
            {"publishYear",  b.publishYear}
        });
    }

    const auto& p = result.params;
    nlohmann::json jQuery = {
        {"title",          p.title},
        {"CAuthor",         p.author},
        {"genre",          p.genre},
        {"series",         p.series},
        {"language",       p.language},
        {"libId",          p.libId},
        {"archiveName",    p.archiveName},
        {"yearFrom",       p.yearFrom},
        {"dateFrom",       p.dateFrom},
        {"dateTo",         p.dateTo},
        {"ratingMin",      p.ratingMin},
        {"withAnnotation", p.withAnnotation},
        {"withCover",      p.withCover},
        {"limit",          p.limit},
        {"offset",         p.offset}
    };

    return {
        {"totalFound", result.totalFound},
        {"query",      jQuery},
        {"books",      jBooks}
    };
}

void CQuerySerializer::SaveToFile(const QueryResult& result, const std::string& path) 
{
    std::ofstream f(path);
    if (!f.is_open())
        throw std::runtime_error("Cannot open output file: " + path);
    f << ToJson(result).dump(2) << "\n";
}

} // namespace Librium::Query






