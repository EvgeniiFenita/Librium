#include "BookQuery.hpp"

#include <sqlite3.h>

#include <sstream>

namespace Librium::Query {

namespace {

std::vector<SAuthorInfo> FetchAuthors(sqlite3* db, int64_t bookId)
{
    std::vector<SAuthorInfo> result;
    const char* sql = R"(
        SELECT a.last_name, a.first_name, a.middle_name
        FROM authors a
        JOIN book_authors ba ON a.id = ba.author_id
        WHERE ba.book_id = ?
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK)
    {
        sqlite3_bind_int64(stmt, 1, bookId);
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            SAuthorInfo ai;
            const auto* ln = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            const auto* fn = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            const auto* mn = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            if (ln) ai.lastName = ln;
            if (fn) ai.firstName = fn;
            if (mn) ai.middleName = mn;
            result.push_back(std::move(ai));
        }
        sqlite3_finalize(stmt);
    }
    return result;
}

std::vector<std::string> FetchGenres(sqlite3* db, int64_t bookId)
{
    std::vector<std::string> result;
    const char* sql = R"(
        SELECT g.code
        FROM genres g
        JOIN book_genres bg ON g.id = bg.genre_id
        WHERE bg.book_id = ?
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK)
    {
        sqlite3_bind_int64(stmt, 1, bookId);
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            const auto* code = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            if (code) result.push_back(code);
        }
        sqlite3_finalize(stmt);
    }
    return result;
}

} // namespace

SQueryResult CBookQuery::Execute(Db::CDatabase& db, const SQueryParams& params)
{
    SQueryResult result;
    result.params = params;

    sqlite3* raw = db.Handle();
    std::stringstream sql;
    sql << "SELECT b.id, b.lib_id, b.title, b.series_no, b.file_size, b.language, b.date_added, b.rating, b.archive_id, b.annotation, arch.name "
        << "FROM books b "
        << "JOIN archives arch ON b.archive_id = arch.id ";

    if (!params.author.empty())
        sql << "JOIN book_authors ba ON b.id = ba.book_id JOIN authors a ON ba.author_id = a.id ";
    if (!params.genre.empty())
        sql << "JOIN book_genres bg ON b.id = bg.book_id JOIN genres g ON bg.genre_id = g.id ";

    sql << "WHERE 1=1 ";

    if (!params.title.empty()) sql << "AND b.title LIKE ? ";
    if (!params.author.empty()) sql << "AND (a.last_name LIKE ? OR a.first_name LIKE ?) ";
    if (!params.genre.empty()) sql << "AND g.code = ? ";
    if (!params.language.empty()) sql << "AND b.language = ? ";
    if (!params.libId.empty()) sql << "AND b.lib_id = ? ";
    if (!params.archiveName.empty()) sql << "AND arch.name = ? ";
    if (!params.dateFrom.empty()) sql << "AND b.date_added >= ? ";
    if (!params.dateTo.empty()) sql << "AND b.date_added <= ? ";
    if (params.ratingMin > 0) sql << "AND b.rating >= ? ";
    if (params.withAnnotation) sql << "AND b.annotation IS NOT NULL AND b.annotation != '' ";

    // Count total matches
    std::string countSql = "SELECT COUNT(*) FROM (" + sql.str() + ")";
    sqlite3_stmt* cstmt = nullptr;
    if (sqlite3_prepare_v2(raw, countSql.c_str(), -1, &cstmt, nullptr) == SQLITE_OK)
    {
        int idx = 1;
        if (!params.title.empty()) sqlite3_bind_text(cstmt, idx++, (params.title + "%").c_str(), -1, SQLITE_TRANSIENT);
        if (!params.author.empty()) 
        {
            sqlite3_bind_text(cstmt, idx++, (params.author + "%").c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(cstmt, idx++, (params.author + "%").c_str(), -1, SQLITE_TRANSIENT);
        }
        if (!params.genre.empty()) sqlite3_bind_text(cstmt, idx++, params.genre.c_str(), -1, SQLITE_TRANSIENT);
        if (!params.language.empty()) sqlite3_bind_text(cstmt, idx++, params.language.c_str(), -1, SQLITE_TRANSIENT);
        if (!params.libId.empty()) sqlite3_bind_text(cstmt, idx++, params.libId.c_str(), -1, SQLITE_TRANSIENT);
        if (!params.archiveName.empty()) sqlite3_bind_text(cstmt, idx++, params.archiveName.c_str(), -1, SQLITE_TRANSIENT);
        if (!params.dateFrom.empty()) sqlite3_bind_text(cstmt, idx++, params.dateFrom.c_str(), -1, SQLITE_TRANSIENT);
        if (!params.dateTo.empty()) sqlite3_bind_text(cstmt, idx++, params.dateTo.c_str(), -1, SQLITE_TRANSIENT);
        if (params.ratingMin > 0) sqlite3_bind_int(cstmt, idx++, params.ratingMin);

        if (sqlite3_step(cstmt) == SQLITE_ROW)
            result.totalFound = sqlite3_column_int64(cstmt, 0);
        sqlite3_finalize(cstmt);
    }

    // Fetch books
    sql << "ORDER BY b.title ";
    if (params.limit > 0) sql << "LIMIT " << params.limit << " OFFSET " << params.offset;

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(raw, sql.str().c_str(), -1, &stmt, nullptr) == SQLITE_OK)
    {
        int idx = 1;
        if (!params.title.empty()) sqlite3_bind_text(stmt, idx++, (params.title + "%").c_str(), -1, SQLITE_TRANSIENT);
        if (!params.author.empty()) 
        {
            sqlite3_bind_text(stmt, idx++, (params.author + "%").c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, idx++, (params.author + "%").c_str(), -1, SQLITE_TRANSIENT);
        }
        if (!params.genre.empty()) sqlite3_bind_text(stmt, idx++, params.genre.c_str(), -1, SQLITE_TRANSIENT);
        if (!params.language.empty()) sqlite3_bind_text(stmt, idx++, params.language.c_str(), -1, SQLITE_TRANSIENT);
        if (!params.libId.empty()) sqlite3_bind_text(stmt, idx++, params.libId.c_str(), -1, SQLITE_TRANSIENT);
        if (!params.archiveName.empty()) sqlite3_bind_text(stmt, idx++, params.archiveName.c_str(), -1, SQLITE_TRANSIENT);
        if (!params.dateFrom.empty()) sqlite3_bind_text(stmt, idx++, params.dateFrom.c_str(), -1, SQLITE_TRANSIENT);
        if (!params.dateTo.empty()) sqlite3_bind_text(stmt, idx++, params.dateTo.c_str(), -1, SQLITE_TRANSIENT);
        if (params.ratingMin > 0) sqlite3_bind_int(stmt, idx++, params.ratingMin);

        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            SBookResult br;
            br.id = sqlite3_column_int64(stmt, 0);
            const auto* lid = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            const auto* tit = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            
            if (lid) br.libId = lid;
            if (tit) br.title = tit;
            br.seriesNumber = sqlite3_column_int(stmt, 3);
            br.fileSize = sqlite3_column_int64(stmt, 4);
            const auto* lang = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
            const auto* dadd = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
            if (lang) br.language = lang;
            if (dadd) br.dateAdded = dadd;
            br.rating = sqlite3_column_int(stmt, 7);
            const auto* ann = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 9));
            const auto* arch = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 10));
            if (ann) br.annotation = ann;
            if (arch) br.archiveName = arch;

            br.authors = FetchAuthors(raw, br.id);
            br.genres = FetchGenres(raw, br.id);

            result.books.push_back(std::move(br));
        }
        sqlite3_finalize(stmt);
    }

    return result;
}

} // namespace Librium::Query
