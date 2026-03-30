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
