#include "BookQuery.hpp"
#include "Database/SqlQueries.hpp"
#include "Database/ISqlDatabase.hpp"
#include "Database/ISqlStatement.hpp"

#include <sstream>

namespace Librium::Query {

namespace {

std::vector<SAuthorInfo> FetchAuthors(Db::ISqlDatabase* db, int64_t bookId)
{
    std::vector<SAuthorInfo> result;
    const char* sql = Db::Sql::FetchBookAuthors.data();

    auto stmt = db->Prepare(std::string(sql));
    stmt->BindInt64(1, bookId);
    stmt->Step();
    while (stmt->IsRow())
    {
        SAuthorInfo ai;
        ai.lastName = stmt->ColumnText(0);
        ai.firstName = stmt->ColumnText(1);
        ai.middleName = stmt->ColumnText(2);
        result.push_back(std::move(ai));
        stmt->Step();
    }
    return result;
}

std::vector<std::string> FetchGenres(Db::ISqlDatabase* db, int64_t bookId)
{
    std::vector<std::string> result;
    const char* sql = Db::Sql::FetchBookGenres.data();

    auto stmt = db->Prepare(std::string(sql));
    stmt->BindInt64(1, bookId);
    stmt->Step();
    while (stmt->IsRow())
    {
        std::string code = stmt->ColumnText(0);
        if (!code.empty()) result.push_back(code);
        stmt->Step();
    }
    return result;
}

void BindParams(Db::ISqlStatement* stmt, const SQueryParams& params)
{
    int idx = 1;
    if (!params.title.empty()) stmt->BindText(idx++, params.title + "%");
    if (!params.author.empty()) 
    {
        stmt->BindText(idx++, params.author + "%");
        stmt->BindText(idx++, params.author + "%");
    }
    if (!params.genre.empty()) stmt->BindText(idx++, params.genre);
    if (!params.series.empty()) stmt->BindText(idx++, params.series + "%");
    if (!params.language.empty()) stmt->BindText(idx++, params.language);
    if (!params.libId.empty()) stmt->BindText(idx++, params.libId);
    if (!params.archiveName.empty()) stmt->BindText(idx++, params.archiveName);
    if (!params.dateFrom.empty()) stmt->BindText(idx++, params.dateFrom);
    if (!params.dateTo.empty()) stmt->BindText(idx++, params.dateTo);
    if (params.ratingMin > 0) stmt->BindInt(idx++, params.ratingMin);
}

SBookResult ReadBookRow(Db::ISqlStatement* stmt, Db::ISqlDatabase* db)
{
    SBookResult br;
    br.id = stmt->ColumnInt64(0);
    
    br.libId        = stmt->ColumnText(1);
    br.title        = stmt->ColumnText(2);
    br.series       = stmt->ColumnText(3);
    br.seriesNumber = stmt->ColumnInt(4); 
    br.fileName     = stmt->ColumnText(5);
    br.fileSize     = stmt->ColumnInt64(6);
    br.fileExt      = stmt->ColumnText(7);
    br.dateAdded    = stmt->ColumnText(8);
    br.language     = stmt->ColumnText(9);
    br.rating       = stmt->ColumnInt(10);
    br.keywords     = stmt->ColumnText(11);
    br.annotation   = stmt->ColumnText(12);
    br.archiveName  = stmt->ColumnText(13);
    br.publisher    = stmt->ColumnText(14);
    br.isbn         = stmt->ColumnText(15);
    br.publishYear  = stmt->ColumnText(16);

    br.authors = FetchAuthors(db, br.id);
    br.genres = FetchGenres(db, br.id);
    return br;
}

} // namespace

SQueryResult CBookQuery::Execute(Db::CDatabase& db, const SQueryParams& params)
{
    SQueryResult result;
    result.params = params;

    Db::ISqlDatabase* raw = db.Handle();
    std::stringstream sql;
    sql << "SELECT b.id, b.lib_id, b.title, ser.name, b.series_no, b.file_name, b.file_size, b.file_ext, b.date_added, "
        << "b.language, b.rating, b.keywords, b.annotation, arch.name, pub.name, b.isbn, b.publish_date "
        << "FROM books b "
        << "JOIN archives arch ON b.archive_id = arch.id "
        << "LEFT JOIN series ser ON b.series_id = ser.id "
        << "LEFT JOIN publishers pub ON b.publisher_id = pub.id ";

    if (!params.author.empty())
        sql << "JOIN book_authors ba ON b.id = ba.book_id JOIN authors a ON ba.author_id = a.id ";
    if (!params.genre.empty())
        sql << "JOIN book_genres bg ON b.id = bg.book_id JOIN genres g ON bg.genre_id = g.id ";

    sql << "WHERE 1=1 ";

    if (!params.title.empty()) sql << "AND b.title LIKE ? ";
    if (!params.author.empty()) sql << "AND (a.last_name LIKE ? OR a.first_name LIKE ?) ";
    if (!params.genre.empty()) sql << "AND g.code = ? ";
    if (!params.series.empty()) sql << "AND ser.name LIKE ? ";
    if (!params.language.empty()) sql << "AND b.language = ? ";
    if (!params.libId.empty()) sql << "AND b.lib_id = ? ";
    if (!params.archiveName.empty()) sql << "AND arch.name = ? ";
    if (!params.dateFrom.empty()) sql << "AND b.date_added >= ? ";
    if (!params.dateTo.empty()) sql << "AND b.date_added <= ? ";
    if (params.ratingMin > 0) sql << "AND b.rating >= ? ";
    if (params.withAnnotation) sql << "AND b.annotation IS NOT NULL AND b.annotation != '' ";

    // Count total matches
    try
    {
        std::string countSql = "SELECT COUNT(*) FROM (" + sql.str() + ")";
        auto cstmt = raw->Prepare(countSql);
        BindParams(cstmt.get(), params);
        cstmt->Step();
        if (cstmt->IsRow())
            result.totalFound = cstmt->ColumnInt64(0);
    }
    catch (const Db::CDbError&)
    {
        // Ignore or log error
    }

    // Fetch books
    sql << "ORDER BY b.title ";
    if (params.limit > 0) sql << "LIMIT " << params.limit << " OFFSET " << params.offset;

    try
    {
        auto stmt = raw->Prepare(sql.str());
        BindParams(stmt.get(), params);

        stmt->Step();
        while (stmt->IsRow())
        {
            result.books.push_back(ReadBookRow(stmt.get(), raw));
            stmt->Step();
        }
    }
    catch (const Db::CDbError&)
    {
        // Ignore or log error
    }

    return result;
}

std::optional<SBookResult> CBookQuery::GetBookById(Db::CDatabase& db, int64_t id)
{
    Db::ISqlDatabase* raw = db.Handle();
    std::stringstream sql;
    sql << "SELECT b.id, b.lib_id, b.title, ser.name, b.series_no, b.file_name, b.file_size, b.file_ext, b.date_added, "
        << "b.language, b.rating, b.keywords, b.annotation, arch.name, pub.name, b.isbn, b.publish_date "
        << "FROM books b "
        << "JOIN archives arch ON b.archive_id = arch.id "
        << "LEFT JOIN series ser ON b.series_id = ser.id "
        << "LEFT JOIN publishers pub ON b.publisher_id = pub.id "
        << "WHERE b.id = ?";

    try
    {
        auto stmt = raw->Prepare(sql.str());
        stmt->BindInt64(1, id);
        stmt->Step();
        if (stmt->IsRow())
        {
            return ReadBookRow(stmt.get(), raw);
        }
    }
    catch (const Db::CDbError&)
    {
        // Ignore or log error
    }
    
    return std::nullopt;
}

} // namespace Librium::Query
