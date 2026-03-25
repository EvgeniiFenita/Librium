#include "Database.hpp"
#include "DatabaseSchema.hpp"
#include "SqlQueries.hpp"
#include "Log/Logger.hpp"
#include "SqliteDatabase.hpp"
#include "SearchQueryParser.hpp"

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
        LOG_INFO("Database restored to WAL mode after bulk import");
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("EndBulkImport failed: {}", e.what());
        throw;
    }
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

int64_t CDatabase::GetOrCreateAuthor(const Inpx::SAuthor& author)
{
    std::string key = author.lastName + "|" + author.firstName + "|" + author.middleName;
    auto it = m_cacheAuthors.find(key);
    if (it != m_cacheAuthors.end()) return it->second;

    m_stmtInsertAuthor->Reset();
    m_stmtInsertAuthor->BindText(1, author.lastName);
    m_stmtInsertAuthor->BindText(2, author.firstName);
    m_stmtInsertAuthor->BindText(3, author.middleName);
    
    std::string searchName = author.lastName;
    if (!author.firstName.empty()) searchName += " " + author.firstName;
    if (!author.middleName.empty()) searchName += " " + author.middleName;
    m_stmtInsertAuthor->BindText(4, searchName);
    
    m_stmtInsertAuthor->Step();

    m_stmtGetAuthor->Reset();
    m_stmtGetAuthor->BindText(1, author.lastName);
    m_stmtGetAuthor->BindText(2, author.firstName);
    m_stmtGetAuthor->BindText(3, author.middleName);
    m_stmtGetAuthor->Step();
    CSqlStmtResetGuard authorGuard(*m_stmtGetAuthor);

    int64_t id = 0;
    if (m_stmtGetAuthor->IsRow())
        id = m_stmtGetAuthor->ColumnInt64(0);

    if (id > 0) m_cacheAuthors[key] = id;
    return id;
}

int64_t CDatabase::GetOrCreateGenre(const std::string& genre)
{
    auto it = m_cacheGenres.find(genre);
    if (it != m_cacheGenres.end()) return it->second;

    m_stmtInsertGenre->Reset();
    m_stmtInsertGenre->BindText(1, genre);
    m_stmtInsertGenre->Step();

    m_stmtGetGenre->Reset();
    m_stmtGetGenre->BindText(1, genre);
    m_stmtGetGenre->Step();
    CSqlStmtResetGuard genreGuard(*m_stmtGetGenre);

    int64_t id = 0;
    if (m_stmtGetGenre->IsRow())
        id = m_stmtGetGenre->ColumnInt64(0);

    if (id > 0) m_cacheGenres[genre] = id;
    return id;
}

int64_t CDatabase::GetOrCreateSeries(const std::string& series)
{
    if (series.empty()) return 0;
    auto it = m_cacheSeries.find(series);
    if (it != m_cacheSeries.end()) return it->second;

    m_stmtInsertSeries->Reset();
    m_stmtInsertSeries->BindText(1, series);
    m_stmtInsertSeries->BindText(2, series); // search_name = librium_upper(param 2)
    m_stmtInsertSeries->Step();

    m_stmtGetSeries->Reset();
    m_stmtGetSeries->BindText(1, series);
    m_stmtGetSeries->Step();
    CSqlStmtResetGuard seriesGuard(*m_stmtGetSeries);

    int64_t id = 0;
    if (m_stmtGetSeries->IsRow())
        id = m_stmtGetSeries->ColumnInt64(0);

    if (id > 0) m_cacheSeries[series] = id;
    return id;
}

int64_t CDatabase::GetOrCreatePublisher(const std::string& pub)
{
    if (pub.empty()) return 0;
    auto it = m_cachePublishers.find(pub);
    if (it != m_cachePublishers.end()) return it->second;

    m_stmtInsertPublisher->Reset();
    m_stmtInsertPublisher->BindText(1, pub);
    m_stmtInsertPublisher->Step();

    m_stmtGetPublisher->Reset();
    m_stmtGetPublisher->BindText(1, pub);
    m_stmtGetPublisher->Step();
    CSqlStmtResetGuard publisherGuard(*m_stmtGetPublisher);

    int64_t id = 0;
    if (m_stmtGetPublisher->IsRow())
        id = m_stmtGetPublisher->ColumnInt64(0);

    if (id > 0) m_cachePublishers[pub] = id;
    return id;
}

int64_t CDatabase::GetOrCreateArchive(const std::string& name)
{
    auto it = m_cacheArchives.find(name);
    if (it != m_cacheArchives.end()) return it->second;

    m_stmtInsertArchive->Reset();
    m_stmtInsertArchive->BindText(1, name);
    m_stmtInsertArchive->Step();

    m_stmtGetArchive->Reset();
    m_stmtGetArchive->BindText(1, name);
    m_stmtGetArchive->Step();
    CSqlStmtResetGuard archiveGuard(*m_stmtGetArchive);

    int64_t id = 0;
    if (m_stmtGetArchive->IsRow())
        id = m_stmtGetArchive->ColumnInt64(0);

    if (id > 0) m_cacheArchives[name] = id;
    return id;
}

int64_t CDatabase::InsertBook(const Inpx::SBookRecord& rec, const Fb2::SFb2Data& fb2)
{
    int64_t archId = GetOrCreateArchive(rec.archiveName);
    int64_t seriesId = GetOrCreateSeries(rec.series);
    int64_t pubId = GetOrCreatePublisher(fb2.publisher);

    m_stmtInsertBook->Reset();
    m_stmtInsertBook->BindText(1, rec.libId);
    m_stmtInsertBook->BindInt64(2, archId);
    m_stmtInsertBook->BindText(3, rec.title);
    m_stmtInsertBook->BindText(4, rec.title); // search_title
    
    if (seriesId > 0) m_stmtInsertBook->BindInt64(5, seriesId);
    else              m_stmtInsertBook->BindNull(5);

    m_stmtInsertBook->BindInt(6, rec.seriesNumber);
    m_stmtInsertBook->BindText(7, rec.fileName);
    m_stmtInsertBook->BindInt64(8, rec.fileSize);
    m_stmtInsertBook->BindText(9, rec.fileExt);
    m_stmtInsertBook->BindText(10, rec.dateAdded);
    m_stmtInsertBook->BindText(11, rec.language);
    m_stmtInsertBook->BindInt(12, rec.rating);
    m_stmtInsertBook->BindText(13, rec.keywords);

    m_stmtInsertBook->BindText(14, fb2.annotation);
    
    if (pubId > 0) m_stmtInsertBook->BindInt64(15, pubId);
    else           m_stmtInsertBook->BindNull(15);

    m_stmtInsertBook->BindText(16, fb2.isbn);
    m_stmtInsertBook->BindText(17, fb2.publishYear);
    
    m_stmtInsertBook->Step();
    if (!m_stmtInsertBook->IsDone())
        return 0;

    int64_t bookId = LastInsertRowId();

    for (const auto& author : rec.authors)
    {
        int64_t aid = GetOrCreateAuthor(author);
        m_stmtInsertBookAuthor->Reset();
        m_stmtInsertBookAuthor->BindInt64(1, bookId);
        m_stmtInsertBookAuthor->BindInt64(2, aid);
        m_stmtInsertBookAuthor->Step();
    }

    for (const auto& genre : rec.genres)
    {
        int64_t gid = GetOrCreateGenre(genre);
        m_stmtInsertBookGenre->Reset();
        m_stmtInsertBookGenre->BindInt64(1, bookId);
        m_stmtInsertBookGenre->BindInt64(2, gid);
        m_stmtInsertBookGenre->Step();
    }

    return bookId;
}

bool CDatabase::BookExists(const std::string& libId, const std::string& archiveName)
{
    int64_t archId = GetOrCreateArchive(archiveName);
    m_stmtBookExists->Reset();
    m_stmtBookExists->BindText(1, libId);
    m_stmtBookExists->BindInt64(2, archId);
    m_stmtBookExists->Step();
    CSqlStmtResetGuard guard(*m_stmtBookExists);
    return m_stmtBookExists->IsRow();
}

std::vector<std::string> CDatabase::GetIndexedArchives()
{
    std::vector<std::string> res;
    auto stmt = m_db->Prepare(std::string(Sql::GetIndexedArchives));

    stmt->Step();
    CSqlStmtResetGuard guard(*stmt);
    while (stmt->IsRow())
    {
        std::string text = stmt->ColumnText(0);
        if (!text.empty()) res.emplace_back(text);
        stmt->Step();
    }
    return res;
}

void CDatabase::MarkArchiveIndexed(const std::string& archiveName)
{
    (void)GetOrCreateArchive(archiveName);
    auto stmt = m_db->Prepare(std::string(Sql::UpdateArchiveIndexed));

    stmt->BindText(1, archiveName);
    stmt->Step();
    CSqlStmtResetGuard guard(*stmt);
}

int64_t CDatabase::CountBooks() const
{
    auto stmt = m_db->Prepare(std::string(Sql::CountBooks));
    stmt->Step();
    CSqlStmtResetGuard guard(*stmt);
    return stmt->IsRow() ? stmt->ColumnInt64(0) : 0;
}

int64_t CDatabase::CountAuthors() const
{
    auto stmt = m_db->Prepare(std::string(Sql::CountAuthors));
    stmt->Step();
    CSqlStmtResetGuard guard(*stmt);
    return stmt->IsRow() ? stmt->ColumnInt64(0) : 0;
}

int CDatabase::CountIndexes() const
{
    auto stmt = m_db->Prepare(std::string(Sql::CountBookIndexes));
    stmt->Step();
    CSqlStmtResetGuard guard(*stmt);
    return stmt->IsRow() ? stmt->ColumnInt(0) : 0;
}

int64_t CDatabase::LastInsertRowId() const
{
    return m_db->LastInsertRowId();
}

std::optional<SBookPath> CDatabase::GetBookPath(int64_t bookId)
{
    m_stmtGetBookPath->Reset();
    m_stmtGetBookPath->BindInt64(1, bookId);
    m_stmtGetBookPath->Step();
    CSqlStmtResetGuard guard(*m_stmtGetBookPath);

    if (m_stmtGetBookPath->IsRow())
    {
        SBookPath bp;
        bp.archiveName = m_stmtGetBookPath->ColumnText(0);
        bp.fileName    = m_stmtGetBookPath->ColumnText(1);
        return bp;
    }
    return std::nullopt;
}

void CDatabase::DropIndexes()
{
    LOG_INFO("Dropping indexes for bulk import...");
    Exec(std::string(Sql::DropIndexBooksTitle));
    Exec(std::string(Sql::DropIndexBooksLang));
    Exec(std::string(Sql::DropIndexBooksSearchTitle));
    Exec(std::string(Sql::DropIndexAuthorsSearchName));
    Exec(std::string(Sql::DropIndexSeriesSearchName));
}

void CDatabase::CreateIndexes()
{
    LOG_INFO("Re-creating indexes after import...");
    Exec(std::string(Sql::CreateIndexBooksTitle));
    Exec(std::string(Sql::CreateIndexBooksLang));
    Exec(std::string(Sql::CreateIndexBooksSearchTitle));
    Exec(std::string(Sql::CreateIndexAuthorsSearchName));
    Exec(std::string(Sql::CreateIndexSeriesSearchName));
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
    if (!params.genre.empty()) stmt->BindText(idx++, params.genre);
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

    std::string joinSql;
    if (!params.author.empty()) joinSql += Sql::QueryJoinAuthors;
    if (!params.genre.empty()) joinSql += Sql::QueryJoinGenres;

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
        std::string frag;
        BuildSearchSql(token, "a.search_name", frag, binds.author);
        whereSql += frag;
    }
    if (!params.series.empty())
    {
        auto token = ParseSearchQuery(params.series);
        std::string frag;
        BuildSearchSql(token, "ser.search_name", frag, binds.series);
        whereSql += frag;
    }

    if (!params.genre.empty()) whereSql += Sql::QueryWhereGenre;
    if (!params.language.empty()) whereSql += Sql::QueryWhereLanguage;
    if (!params.libId.empty()) whereSql += Sql::QueryWhereLibId;
    if (!params.archiveName.empty()) whereSql += Sql::QueryWhereArchiveName;
    if (!params.dateFrom.empty()) whereSql += Sql::QueryWhereDateFrom;
    if (!params.dateTo.empty()) whereSql += Sql::QueryWhereDateTo;
    if (params.ratingMin > 0) whereSql += Sql::QueryWhereRatingMin;
    if (params.ratingMax > 0) whereSql += Sql::QueryWhereRatingMax;
    if (params.withAnnotation) whereSql += Sql::QueryWhereWithAnnotation;

    // Count total matches
    try
    {
        std::string fullCountSql = countSql + joinSql + whereSql;
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
        std::string fullSelectSql = selectSql + joinSql + whereSql + orderLimitSql;
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
