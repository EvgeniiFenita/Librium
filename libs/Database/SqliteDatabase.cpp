#include "SqliteDatabase.hpp"
#include "SqliteStatement.hpp"
#include "SqliteFunctions.hpp"
#include "Database.hpp" // For CDbError
#include "Log/Logger.hpp"

#include <sqlite3.h>
#include <stdexcept>

namespace Librium::Db {

CSqliteDatabase::CSqliteDatabase(const std::string& path, int64_t cacheSize, int64_t mmapSize)
    : m_db(nullptr, [](sqlite3* db) { if (db) sqlite3_close(db); })
{
    LOG_INFO("Opening SQLite database: {}", path);
    sqlite3* rawDb = nullptr;
    if (sqlite3_open(path.c_str(), &rawDb) != SQLITE_OK)
    {
        std::string err = rawDb ? sqlite3_errmsg(rawDb) : "Unknown error";
        if (rawDb) sqlite3_close(rawDb);
        throw CDbError("Cannot open database: " + path + " - " + err);
    }
    m_db.reset(rawDb);

    RegisterSqliteFunctions(m_db.get());

    // Performance optimizations
    Exec("PRAGMA cache_size = " + std::to_string(cacheSize));
    Exec("PRAGMA temp_store = MEMORY");
    Exec("PRAGMA mmap_size = " + std::to_string(mmapSize));
    Exec("PRAGMA busy_timeout = 5000");
}

CSqliteDatabase::~CSqliteDatabase() = default;

void CSqliteDatabase::Exec(const std::string& sql)
{
    LOG_DEBUG("Executing SQL: {}", sql);
    char* err = nullptr;
    if (sqlite3_exec(m_db.get(), sql.c_str(), nullptr, nullptr, &err) != SQLITE_OK)
    {
        std::string msg = err ? err : "Unknown error";
        if (err) sqlite3_free(err);
        LOG_ERROR("SQL error: {} | Query: {}", msg, sql);
        throw CDbError("SQL error: " + msg);
    }
}

std::unique_ptr<ISqlStatement> CSqliteDatabase::Prepare(const std::string& sql)
{
    LOG_DEBUG("Preparing SQL: {}", sql);
    sqlite3_stmt* raw = nullptr;
    if (sqlite3_prepare_v2(m_db.get(), sql.c_str(), -1, &raw, nullptr) != SQLITE_OK)
    {
        std::string msg = sqlite3_errmsg(m_db.get());
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
    return sqlite3_last_insert_rowid(m_db.get());
}

} // namespace Librium::Db
