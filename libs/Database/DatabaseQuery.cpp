#include "Database.hpp"
#include "SqlQueries.hpp"
#include "SearchQueryParser.hpp"
#include "Utils/GenreTranslator.hpp"

#include <algorithm>
#include <sstream>
#include <unordered_map>

namespace Librium::Db {

namespace {

constexpr size_t kBookBatchLoadChunkSize = 400;

std::string BuildIdPlaceholders(size_t count)
{
    std::string placeholders;
    placeholders.reserve(count * 3);
    for (size_t i = 0; i < count; ++i)
    {
        if (i > 0) placeholders += ", ";
        placeholders += "?";
    }
    return placeholders;
}

std::unordered_map<int64_t, size_t> BuildBookIndex(const std::vector<SBookResult>& books)
{
    std::unordered_map<int64_t, size_t> bookIndex;
    bookIndex.reserve(books.size());
    for (size_t i = 0; i < books.size(); ++i)
    {
        bookIndex.emplace(books[i].id, i);
    }
    return bookIndex;
}

template<typename TLoadChunk>
void LoadBookChunks(std::vector<SBookResult>& books, TLoadChunk&& loadChunk)
{
    for (size_t start = 0; start < books.size(); start += kBookBatchLoadChunkSize)
    {
        const size_t count = std::min(kBookBatchLoadChunkSize, books.size() - start);
        loadChunk(start, count);
    }
}

void LoadAuthorsForBooks(ISqlDatabase* db, std::vector<SBookResult>& books)
{
    if (books.empty()) return;

    const auto bookIndex = BuildBookIndex(books);
    LoadBookChunks(books, [&](size_t start, size_t count)
    {
        auto sql = std::string(
            "SELECT ba.book_id, a.last_name, a.first_name, a.middle_name "
            "FROM book_authors ba "
            "JOIN authors a ON a.id = ba.author_id "
            "WHERE ba.book_id IN ("
        );
        sql += BuildIdPlaceholders(count);
        sql += ")";

        auto stmt = db->Prepare(sql);
        for (size_t i = 0; i < count; ++i)
            stmt->BindInt64(static_cast<int>(i + 1), books[start + i].id);

        stmt->Step();
        CSqlStmtResetGuard guard(*stmt);
        while (stmt->IsRow())
        {
            const int64_t bookId = stmt->ColumnInt64(0);
            const auto it = bookIndex.find(bookId);
            if (it != bookIndex.end())
            {
                SAuthorInfo ai;
                ai.lastName = stmt->ColumnText(1);
                ai.firstName = stmt->ColumnText(2);
                ai.middleName = stmt->ColumnText(3);
                books[it->second].authors.push_back(std::move(ai));
            }
            stmt->Step();
        }
    });
}

void LoadGenresForBooks(ISqlDatabase* db, std::vector<SBookResult>& books)
{
    if (books.empty()) return;

    const auto bookIndex = BuildBookIndex(books);
    LoadBookChunks(books, [&](size_t start, size_t count)
    {
        auto sql = std::string(
            "SELECT bg.book_id, g.code "
            "FROM book_genres bg "
            "JOIN genres g ON g.id = bg.genre_id "
            "WHERE bg.book_id IN ("
        );
        sql += BuildIdPlaceholders(count);
        sql += ")";

        auto stmt = db->Prepare(sql);
        for (size_t i = 0; i < count; ++i)
            stmt->BindInt64(static_cast<int>(i + 1), books[start + i].id);

        stmt->Step();
        CSqlStmtResetGuard guard(*stmt);
        while (stmt->IsRow())
        {
            const int64_t bookId = stmt->ColumnInt64(0);
            const auto it = bookIndex.find(bookId);
            if (it != bookIndex.end())
            {
                std::string code = stmt->ColumnText(1);
                if (!code.empty())
                    books[it->second].genres.push_back(std::move(code));
            }
            stmt->Step();
        }
    });
}

struct SQueryBindValues
{
    std::string title;
    std::string author;
    std::string series;
};

int BindQueryParamsCustom(ISqlStatement* stmt, const SQueryParams& params, const SQueryBindValues& binds)
{
    int idx = 1;
    if (!binds.title.empty()) stmt->BindText(idx++, binds.title);
    if (!binds.author.empty()) stmt->BindText(idx++, binds.author);
    if (!binds.series.empty()) stmt->BindText(idx++, binds.series);
    if (!params.genre.empty()) stmt->BindText(idx++, Utils::CGenreTranslator::Translate(params.genre));
    if (!params.language.empty()) stmt->BindText(idx++, params.language);
    if (!params.libId.empty()) stmt->BindText(idx++, params.libId);
    if (!params.archiveName.empty()) stmt->BindText(idx++, params.archiveName);
    if (!params.dateFrom.empty()) stmt->BindText(idx++, params.dateFrom);
    if (!params.dateTo.empty()) stmt->BindText(idx++, params.dateTo);
    if (params.ratingMin > 0) stmt->BindInt(idx++, params.ratingMin);
    if (params.ratingMax > 0) stmt->BindInt(idx++, params.ratingMax);
    return idx;
}

SBookResult ReadBookRow(ISqlStatement* stmt)
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
    return br;
}

} // namespace

SQueryResult CDatabase::ExecuteQuery(const SQueryParams& params)
{
    SQueryResult result;
    result.params = params;

    std::string selectSql{Sql::QuerySelectBooksBase};
    std::string countSql{Sql::QueryCountBooksBase};

    std::string whereSql{Sql::QueryWhere1};
    SQueryBindValues binds;

    if (!params.title.empty())
    {
        auto token = ParseSearchQuery(params.title);
        std::string frag;
        BuildSearchSql(token, "b.search_title", frag, binds.title);
        whereSql += frag;
    }
    if (!params.author.empty())
    {
        auto token = ParseSearchQuery(params.author);
        std::string cond;
        BuildSearchSql(token, "a.search_name", cond, binds.author);
        whereSql += " AND EXISTS (SELECT 1 FROM book_authors ba "
                    "JOIN authors a ON ba.author_id = a.id "
                    "WHERE ba.book_id = b.id" + cond + ") ";
    }
    if (!params.series.empty())
    {
        auto token = ParseSearchQuery(params.series);
        std::string frag;
        BuildSearchSql(token, "ser.search_name", frag, binds.series);
        whereSql += frag;
    }

    if (!params.genre.empty())
        whereSql += " AND EXISTS (SELECT 1 FROM book_genres bg "
                    "JOIN genres g ON bg.genre_id = g.id "
                    "WHERE bg.book_id = b.id AND g.code = ?) ";
    if (!params.language.empty()) whereSql += Sql::QueryWhereLanguage;
    if (!params.libId.empty()) whereSql += Sql::QueryWhereLibId;
    if (!params.archiveName.empty()) whereSql += Sql::QueryWhereArchiveName;
    if (!params.dateFrom.empty()) whereSql += Sql::QueryWhereDateFrom;
    if (!params.dateTo.empty()) whereSql += Sql::QueryWhereDateTo;
    if (params.ratingMin > 0) whereSql += Sql::QueryWhereRatingMin;
    if (params.ratingMax > 0) whereSql += Sql::QueryWhereRatingMax;
    if (params.withAnnotation) whereSql += Sql::QueryWhereWithAnnotation;

    try
    {
        std::string fullCountSql = countSql + whereSql;
        auto cstmt = m_db->Prepare(fullCountSql);
        BindQueryParamsCustom(cstmt.get(), params, binds);
        cstmt->Step();
        CSqlStmtResetGuard countGuard(*cstmt);
        result.totalFound = cstmt->IsRow() ? cstmt->ColumnInt64(0) : 0;
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("Failed to count books for query: {}", e.what());
    }

    std::string orderLimitSql{Sql::QueryOrderTitle};
    const bool hasLimit = params.limit > 0;
    if (hasLimit)
        orderLimitSql += Sql::QueryLimitOffset;

    try
    {
        std::string fullSelectSql = selectSql + whereSql + orderLimitSql;
        auto stmt = m_db->Prepare(fullSelectSql);
        const int nextIdx = BindQueryParamsCustom(stmt.get(), params, binds);
        if (hasLimit)
        {
            stmt->BindInt64(nextIdx, static_cast<int64_t>(params.limit));
            stmt->BindInt64(nextIdx + 1, static_cast<int64_t>(params.offset));
        }

        stmt->Step();
        CSqlStmtResetGuard selectGuard(*stmt);
        while (stmt->IsRow())
        {
            result.books.push_back(ReadBookRow(stmt.get()));
            stmt->Step();
        }

        LoadAuthorsForBooks(m_db.get(), result.books);
        LoadGenresForBooks(m_db.get(), result.books);
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("Failed to execute book query: {}", e.what());
    }

    return result;
}

std::optional<SBookResult> CDatabase::GetBookById(int64_t id)
{
    std::string fullSql = std::string(Sql::QuerySelectBooksBase) + std::string(Sql::QueryWhereId);

    try
    {
        auto stmt = m_db->Prepare(fullSql);
        stmt->BindInt64(1, id);
        stmt->Step();
        CSqlStmtResetGuard guard(*stmt);

        std::optional<SBookResult> result;
        if (stmt->IsRow())
        {
            result = ReadBookRow(stmt.get());
            std::vector<SBookResult> books;
            books.push_back(*result);
            LoadAuthorsForBooks(m_db.get(), books);
            LoadGenresForBooks(m_db.get(), books);
            result = std::move(books.front());
        }
        return result;
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("Failed to get book by id {}: {}", id, e.what());
    }

    return std::nullopt;
}

} // namespace Librium::Db
