#include "Database.hpp"
#include "SqlQueries.hpp"
#include "Log/Logger.hpp"

namespace Librium::Db {

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

} // namespace Librium::Db
