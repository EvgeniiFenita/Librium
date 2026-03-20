#include "Database.hpp"
#include "DatabaseSchema.hpp"
#include "SqlQueries.hpp"
#include "Log/Logger.hpp"
#include "CSqliteDatabase.hpp"

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
    m_stmtInsertAuthor->Step();

    m_stmtGetAuthor->Reset();
    m_stmtGetAuthor->BindText(1, author.lastName);
    m_stmtGetAuthor->BindText(2, author.firstName);
    m_stmtGetAuthor->BindText(3, author.middleName);
    
    int64_t id = 0;
    m_stmtGetAuthor->Step();
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
    
    int64_t id = 0;
    m_stmtGetGenre->Step();
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
    m_stmtInsertSeries->Step();

    m_stmtGetSeries->Reset();
    m_stmtGetSeries->BindText(1, series);
    
    int64_t id = 0;
    m_stmtGetSeries->Step();
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
    
    int64_t id = 0;
    m_stmtGetPublisher->Step();
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
    
    int64_t id = 0;
    m_stmtGetArchive->Step();
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
    
    if (seriesId > 0) m_stmtInsertBook->BindInt64(4, seriesId);
    else              m_stmtInsertBook->BindNull(4);

    m_stmtInsertBook->BindInt(5, rec.seriesNumber);
    m_stmtInsertBook->BindText(6, rec.fileName);
    m_stmtInsertBook->BindInt64(7, rec.fileSize);
    m_stmtInsertBook->BindText(8, rec.fileExt);
    m_stmtInsertBook->BindText(9, rec.dateAdded);
    m_stmtInsertBook->BindText(10, rec.language);
    m_stmtInsertBook->BindInt(11, rec.rating);
    m_stmtInsertBook->BindText(12, rec.keywords);

    m_stmtInsertBook->BindText(13, fb2.annotation);
    
    if (pubId > 0) m_stmtInsertBook->BindInt64(14, pubId);
    else           m_stmtInsertBook->BindNull(14);

    m_stmtInsertBook->BindText(15, fb2.isbn);
    m_stmtInsertBook->BindText(16, fb2.publishYear);
    
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
    return m_stmtBookExists->IsRow();
}

std::vector<std::string> CDatabase::GetIndexedArchives()
{
    std::vector<std::string> res;
    auto stmt = m_db->Prepare(std::string(Sql::GetIndexedArchives));

    stmt->Step();
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
}

int64_t CDatabase::CountBooks() const
{
    auto stmt = m_db->Prepare(std::string(Sql::CountBooks));
    stmt->Step();
    if (stmt->IsRow())
        return stmt->ColumnInt64(0);
    return 0;
}

int64_t CDatabase::CountAuthors() const
{
    auto stmt = m_db->Prepare(std::string(Sql::CountAuthors));
    stmt->Step();
    if (stmt->IsRow())
        return stmt->ColumnInt64(0);
    return 0;
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
    
    if (m_stmtGetBookPath->IsRow())
    {
        SBookPath bp;
        bp.archiveName = m_stmtGetBookPath->ColumnText(0);
        bp.fileName = m_stmtGetBookPath->ColumnText(1);
        return bp;
    }
    return std::nullopt;
}

void CDatabase::DropIndexes()
{
    LOG_INFO("Dropping indexes for bulk import...");
    Exec(std::string(Sql::DropIndexBooksTitle));
    Exec(std::string(Sql::DropIndexBooksLang));
}

void CDatabase::CreateIndexes()
{
    LOG_INFO("Re-creating indexes after import...");
    Exec(std::string(Sql::CreateIndexBooksTitle));
    Exec(std::string(Sql::CreateIndexBooksLang));
}

} // namespace Librium::Db
