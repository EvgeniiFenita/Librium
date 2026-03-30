#include "Database.hpp"
#include "DatabaseSchema.hpp"
#include "SqlQueries.hpp"
#include "Log/Logger.hpp"
#include "SqliteDatabase.hpp"
#include "SearchQueryParser.hpp"
#include "Utils/GenreTranslator.hpp"

#include <filesystem>
#include <sstream>

namespace Librium::Db {

CDatabase::CDatabase(const std::string& path, const Config::SImportConfig& cfg)
{
    m_db = std::make_unique<CSqliteDatabase>(path, cfg.sqliteCacheSize, cfg.sqliteMmapSize);

    CDatabaseSchema::Create(*m_db);
    PrepareStatements();
}

void CDatabase::BeginTransaction()
{
    m_db->BeginTransaction();
}

void CDatabase::Commit()
{
    m_db->Commit();
}

void CDatabase::Rollback()
{
    m_db->Rollback();
}

void CDatabase::BeginBulkImport()
{
    try
    {
        m_db->Exec(Sql::PragmaJournalOff);
        m_db->Exec(Sql::PragmaSyncOff);
        LOG_INFO("Database switched to bulk import mode (journal=OFF, sync=OFF)");
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("BeginBulkImport failed: {}", e.what());
        throw;
    }
}

void CDatabase::EndBulkImport()
{
    try
    {
        m_db->Exec(Sql::PragmaWal);
        m_db->Exec(Sql::PragmaSyncNormal);
        m_db->ReleaseMemory();
        LOG_INFO("Database restored to WAL mode after bulk import");
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("EndBulkImport failed: {}", e.what());
        throw;
    }
}

void CDatabase::ClearImportCaches()
{
    ResetImportCache(m_cacheArchives);
    ResetImportCache(m_cacheGenres);
    ResetImportCache(m_cacheSeries);
    ResetImportCache(m_cachePublishers);
    ResetImportCache(m_cacheAuthors);
    LOG_INFO("Import caches cleared after bulk import");
}

void CDatabase::Exec(const std::string& sql)
{
    m_db->Exec(sql);
}

void CDatabase::PrepareStatements()
{
    auto prep = [&](std::string_view sql, std::unique_ptr<ISqlStatement>& stmt)
    {
        stmt = m_db->Prepare(std::string(sql));
    };

    prep(Sql::InsertAuthor, m_stmtInsertAuthor);
    prep(Sql::GetAuthorId, m_stmtGetAuthor);
    prep(Sql::InsertGenre, m_stmtInsertGenre);
    prep(Sql::GetGenreId, m_stmtGetGenre);
    prep(Sql::InsertSeries, m_stmtInsertSeries);
    prep(Sql::GetSeriesId, m_stmtGetSeries);
    prep(Sql::InsertArchive, m_stmtInsertArchive);
    prep(Sql::GetArchiveId, m_stmtGetArchive);
    prep(Sql::InsertBook, m_stmtInsertBook);
    prep(Sql::InsertBookAuthor, m_stmtInsertBookAuthor);
    prep(Sql::InsertBookGenre, m_stmtInsertBookGenre);
    prep(Sql::CheckBookExists, m_stmtBookExists);
    prep(Sql::UpdateBookFb2, m_stmtUpdateFb2);
    prep(Sql::GetBookPath, m_stmtGetBookPath);
    
    // Publishers
    prep(Sql::InsertPublisher, m_stmtInsertPublisher);
    prep(Sql::GetPublisherId, m_stmtGetPublisher);
}

int64_t CDatabase::LastInsertRowId() const
{
    return m_db->LastInsertRowId();
}

void CDatabase::ResetImportCache(std::unordered_map<std::string, int64_t>& cache)
{
    std::unordered_map<std::string, int64_t> empty;
    cache.swap(empty);
}

namespace {

std::vector<SAuthorInfo> FetchAuthors(ISqlDatabase* db, int64_t bookId)
{
    std::vector<SAuthorInfo> result;
    auto stmt = db->Prepare(std::string(Sql::FetchBookAuthors));
    stmt->BindInt64(1, bookId);
    stmt->Step();
    CSqlStmtResetGuard guard(*stmt);
    while (stmt->IsRow())
    {
        SAuthorInfo ai;
        ai.lastName   = stmt->ColumnText(0);
        ai.firstName  = stmt->ColumnText(1);
        ai.middleName = stmt->ColumnText(2);
        result.push_back(std::move(ai));
        stmt->Step();
    }
    return result;
}

std::vector<std::string> FetchGenres(ISqlDatabase* db, int64_t bookId)
{
    std::vector<std::string> result;
    auto stmt = db->Prepare(std::string(Sql::FetchBookGenres));
    stmt->BindInt64(1, bookId);
    stmt->Step();
    CSqlStmtResetGuard guard(*stmt);
    while (stmt->IsRow())
    {
        std::string code = stmt->ColumnText(0);
        if (!code.empty()) result.push_back(code);
        stmt->Step();
    }
    return result;
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

SBookResult ReadBookRow(ISqlStatement* stmt, ISqlDatabase* db)
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
        // Use EXISTS to avoid row multiplication when a book has multiple authors
        // that all match the search term (e.g. two authors named "Александр").
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

    // Use EXISTS for genre to avoid row multiplication when a book belongs to
    // multiple genres that all match the filter.
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

    // Count total matches — no JOINs needed so COUNT(*) is correct
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

    // Fetch books
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
            stmt->BindInt64(nextIdx,     static_cast<int64_t>(params.limit));
            stmt->BindInt64(nextIdx + 1, static_cast<int64_t>(params.offset));
        }

        stmt->Step();
        CSqlStmtResetGuard selectGuard(*stmt);
        while (stmt->IsRow())
        {
            result.books.push_back(ReadBookRow(stmt.get(), m_db.get()));
            stmt->Step();
        }
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
            result = ReadBookRow(stmt.get(), m_db.get());
        return result;
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("Failed to get book by id {}: {}", id, e.what());
    }
    
    return std::nullopt;
}

} // namespace Librium::Db
