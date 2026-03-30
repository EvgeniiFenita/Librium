#include "Database.hpp"
#include "DatabaseSchema.hpp"
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
