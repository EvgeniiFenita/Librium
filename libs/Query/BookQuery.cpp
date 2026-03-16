#include "BookQuery.hpp"

#include <sqlite3.h>

#include <sstream>

namespace Librium::Query {

namespace {

std::string Like(const std::string& s) 
{
    // Escape % and _ in LIKE patterns, then wrap with %
    std::string out;
    for (char c : s) 
{
        if (c == '%' || c == '_' || c == '\\') out += '\\';
        out += c;
    }
    return "%" + out + "%";
}

std::vector<AuthorInfo> FetchAuthors(sqlite3* db, int64_t bookId) 
{
    std::vector<AuthorInfo> result;
    sqlite3_stmt* s = nullptr;
    sqlite3_prepare_v2(db,
        "SELECT a.last_name, a.first_name, a.middle_name "
        "FROM authors a JOIN book_authors ba ON ba.author_id=a.id "
        "WHERE ba.book_id=?", -1, &s, nullptr);
    sqlite3_bind_int64(s, 1, bookId);
    while (sqlite3_step(s) == SQLITE_ROW) 
{
        AuthorInfo ai;
        auto col = [&](int i) -> std::string
        {
            const auto* t = reinterpret_cast<const char*>(sqlite3_column_text(s, i));
            return t ? t : "";
        };
        ai.lastName   = col(0);
        ai.firstName  = col(1);
        ai.middleName = col(2);
        result.push_back(std::move(ai));
    }
    sqlite3_finalize(s);
    return result;
}

std::vector<std::string> FetchGenres(sqlite3* db, int64_t bookId) 
{
    std::vector<std::string> result;
    sqlite3_stmt* s = nullptr;
    sqlite3_prepare_v2(db,
        "SELECT g.code FROM genres g JOIN book_genres bg ON bg.genre_id=g.id "
        "WHERE bg.book_id=?", -1, &s, nullptr);
    sqlite3_bind_int64(s, 1, bookId);
    while (sqlite3_step(s) == SQLITE_ROW) 
{
        const auto* t = reinterpret_cast<const char*>(sqlite3_column_text(s, 0));
        if (t) result.emplace_back(t);
    }
    sqlite3_finalize(s);
    return result;
}

} // namespace

QueryResult CBookQuery::Execute(Db::CDatabase& db, const QueryParams& params) 
{
    QueryResult result;
    result.params = params;

    // Build WHERE conditions
    std::vector<std::string> conditions;
    std::vector<std::string> likeValues;  // for LIKE params

    if (!params.title.empty())
        conditions.push_back("b.title LIKE ? ESCAPE '\\'");
    if (!params.language.empty())
        conditions.push_back("b.language = ?");
    if (!params.libId.empty())
        conditions.push_back("b.lib_id = ?");
    if (!params.archiveName.empty())
        conditions.push_back("ar.name = ?");
    if (!params.dateFrom.empty())
        conditions.push_back("b.date_added >= ?");
    if (!params.dateTo.empty())
        conditions.push_back("b.date_added <= ?");
    if (!params.yearFrom.empty())
        conditions.push_back("b.publish_year >= ?");
    if (params.ratingMin > 0)
        conditions.push_back("b.rating >= ?");
    if (params.withAnnotation)
        conditions.push_back("b.annotation != ''");
    if (params.withCover)
        conditions.push_back("b.cover IS NOT NULL");

    // CAuthor needs a subquery
    if (!params.author.empty())
        conditions.push_back(
            "b.id IN (SELECT ba.book_id FROM book_authors ba "
            "JOIN authors a ON a.id=ba.author_id "
            "WHERE a.last_name LIKE ? ESCAPE '\\' OR a.first_name LIKE ? ESCAPE '\\')");

    // Genre needs a join/subquery
    if (!params.genre.empty())
        conditions.push_back(
            "b.id IN (SELECT bg.book_id FROM book_genres bg "
            "JOIN genres g ON g.id=bg.genre_id WHERE g.code = ?)");

    // Series
    if (!params.series.empty())
        conditions.push_back(
            "b.series_id IN (SELECT id FROM series WHERE name LIKE ? ESCAPE '\\')");

    std::string whereClause;
    if (!conditions.empty()) 
{
        whereClause = " WHERE ";
        for (size_t i = 0; i < conditions.size(); ++i) 
{
            if (i > 0) whereClause += " AND ";
            whereClause += conditions[i];
        }
    }

    std::string baseQuery =
        "FROM books b "
        "LEFT JOIN archives ar ON ar.id = b.archive_id "
        + whereClause;

    // Bind helper
    auto bindParams = [&](sqlite3_stmt* s) 
{
        int idx = 1;
        if (!params.title.empty()) 
{
            std::string val = Like(params.title);
            sqlite3_bind_text(s, idx++, val.c_str(), -1, SQLITE_TRANSIENT);
        }
        if (!params.language.empty()) 
{
            sqlite3_bind_text(s, idx++, params.language.c_str(), -1, SQLITE_TRANSIENT);
        }
        if (!params.libId.empty()) 
{
            sqlite3_bind_text(s, idx++, params.libId.c_str(), -1, SQLITE_TRANSIENT);
        }
        if (!params.archiveName.empty()) 
{
            sqlite3_bind_text(s, idx++, params.archiveName.c_str(), -1, SQLITE_TRANSIENT);
        }
        if (!params.dateFrom.empty()) 
{
            sqlite3_bind_text(s, idx++, params.dateFrom.c_str(), -1, SQLITE_TRANSIENT);
        }
        if (!params.dateTo.empty()) 
{
            sqlite3_bind_text(s, idx++, params.dateTo.c_str(), -1, SQLITE_TRANSIENT);
        }
        if (!params.yearFrom.empty()) 
{
            sqlite3_bind_text(s, idx++, params.yearFrom.c_str(), -1, SQLITE_TRANSIENT);
        }
        if (params.ratingMin > 0) 
{
            sqlite3_bind_int(s, idx++, params.ratingMin);
        }
        if (!params.author.empty()) 
{
            auto lk = Like(params.author);
            sqlite3_bind_text(s, idx++, lk.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(s, idx++, lk.c_str(), -1, SQLITE_TRANSIENT);
        }
        if (!params.genre.empty()) 
{
            sqlite3_bind_text(s, idx++, params.genre.c_str(), -1, SQLITE_TRANSIENT);
        }
        if (!params.series.empty()) 
{
            std::string val = Like(params.series);
            sqlite3_bind_text(s, idx++, val.c_str(), -1, SQLITE_TRANSIENT);
        }
    };

    sqlite3* raw = db.Handle();

    // COUNT query
    {
        std::string countSql = "SELECT COUNT(*) " + baseQuery;
        sqlite3_stmt* s = nullptr;
        sqlite3_prepare_v2(raw, countSql.c_str(), -1, &s, nullptr);
        bindParams(s);
        if (sqlite3_step(s) == SQLITE_ROW)
            result.totalFound = sqlite3_column_int64(s, 0);
        sqlite3_finalize(s);
    }

    // SELECT query
    {
        std::string selectSql =
            "SELECT b.id, b.lib_id, b.title, "
            "COALESCE(se.name,''), b.series_number, b.language, "
            "b.date_added, b.rating, b.file_size, COALESCE(ar.name,''), "
            "b.annotation, COALESCE(pu.name,''), b.isbn, b.publish_year "
            + baseQuery +
            " ORDER BY b.id";

        if (params.limit > 0)
            selectSql += " LIMIT " + std::to_string(params.limit);
        if (params.offset > 0)
            selectSql += " OFFSET " + std::to_string(params.offset);

        // Add joins for series and publisher
        size_t fromPos = selectSql.find("FROM books b");
        selectSql.insert(fromPos + 12,
            " LEFT JOIN series    se ON se.id = b.series_id"
            " LEFT JOIN publishers pu ON pu.id = b.publisher_id");

        sqlite3_stmt* s = nullptr;
        sqlite3_prepare_v2(raw, selectSql.c_str(), -1, &s, nullptr);
        bindParams(s);

        while (sqlite3_step(s) == SQLITE_ROW) 
{
            auto col = [&](int i) -> std::string
            {
                const auto* t = reinterpret_cast<const char*>(sqlite3_column_text(s, i));
                return t ? t : "";
            };

            BookResult br;
            int64_t bookDbId  = sqlite3_column_int64(s, 0);
            br.libId          = col(1);
            br.title          = col(2);
            br.series         = col(3);
            br.seriesNumber   = sqlite3_column_int(s, 4);
            br.language       = col(5);
            br.dateAdded      = col(6);
            br.rating         = sqlite3_column_int(s, 7);
            br.fileSize       = static_cast<uint64_t>(sqlite3_column_int64(s, 8));
            br.archiveName    = col(9);
            br.annotation     = col(10);
            br.publisher      = col(11);
            br.isbn           = col(12);
            br.publishYear    = col(13);

            br.authors = FetchAuthors(raw, bookDbId);
            br.genres  = FetchGenres(raw,  bookDbId);

            result.books.push_back(std::move(br));
        }
        sqlite3_finalize(s);
    }

    return result;
}

} // namespace Librium::Query






