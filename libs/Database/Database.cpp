#include "Database.hpp"
#include "DatabaseSchema.hpp"
#include "SqlQueries.hpp"
#include "Log/Logger.hpp"
#include "SqliteDatabase.hpp"
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

} // namespace Librium::Db
