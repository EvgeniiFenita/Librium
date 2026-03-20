#include "CSqliteDatabase.hpp"
#include "CSqliteStatement.hpp"
#include "Database.hpp" // For CDbError
#include "Log/Logger.hpp"

#include <sqlite3.h>
#include <stdexcept>

namespace Librium::Db {

CSqliteDatabase::CSqliteDatabase(const std::string& path, int64_t cacheSize, int64_t mmapSize)
    : m_db(nullptr)
{
    LOG_INFO("Opening SQLite database: {}", path);
    if (sqlite3_open(path.c_str(), &m_db) != SQLITE_OK)
    {
        std::string err = m_db ? sqlite3_errmsg(m_db) : "Unknown error";
        if (m_db) sqlite3_close(m_db);
        throw CDbError("Cannot open database: " + path + " - " + err);
    }

    // Performance optimizations
    Exec("PRAGMA cache_size = " + std::to_string(cacheSize));
    Exec("PRAGMA page_size = 4096");
    Exec("PRAGMA temp_store = MEMORY");
    Exec("PRAGMA mmap_size = " + std::to_string(mmapSize));
}

CSqliteDatabase::~CSqliteDatabase()
{
    if (m_db)
    {
        sqlite3_close(m_db);
    }
}

void CSqliteDatabase::Exec(const std::string& sql)
{
    LOG_DEBUG("Executing SQL: {}", sql);
    char* err = nullptr;
    if (sqlite3_exec(m_db, sql.c_str(), nullptr, nullptr, &err) != SQLITE_OK)
    {
        std::string msg = err ? err : "Unknown error";
        if (err) sqlite3_free(err);
        LOG_ERROR("SQL error: {} | Query: {}", msg, sql);
        throw CDbError("SQL error: " + msg);
    }
}

std::unique_ptr<ISqlStatement> CSqliteDatabase::Prepare(const std::string& sql)
{
    sqlite3_stmt* raw = nullptr;
    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &raw, nullptr) != SQLITE_OK)
    {
        std::string msg = sqlite3_errmsg(m_db);
        throw CDbError("Failed to prepare statement: " + msg + " | Query: " + sql);
    }
    return std::make_unique<CSqliteStatement>(raw);
}

void CSqliteDatabase::BeginTransaction()
{
    Exec("BEGIN TRANSACTION");
}

void CSqliteDatabase::Commit()
{
    Exec("COMMIT");
}

void CSqliteDatabase::Rollback()
{
    Exec("ROLLBACK");
}

int64_t CSqliteDatabase::LastInsertRowId() const
{
    return sqlite3_last_insert_rowid(m_db);
}

} // namespace Librium::Db
