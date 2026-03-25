#include "SqliteDatabase.hpp"
#include "SqliteStatement.hpp"
#include "SqliteFunctions.hpp"
#include "Database.hpp" // For CDbError
#include "SqlQueries.hpp"
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
    Exec(std::string(Sql::PragmaCacheSizePrefix) + std::to_string(cacheSize));
    Exec(Sql::PragmaTempStore);
    Exec(std::string(Sql::PragmaMmapSizePrefix) + std::to_string(mmapSize));
    Exec(Sql::PragmaBusyTimeout);
}

CSqliteDatabase::~CSqliteDatabase() = default;

void CSqliteDatabase::Exec(std::string_view sql)
{
    LOG_DEBUG("Executing SQL: {}", sql);
    const std::string sqlStr(sql);
    char* err = nullptr;
    if (sqlite3_exec(m_db.get(), sqlStr.c_str(), nullptr, nullptr, &err) != SQLITE_OK)
    {
        std::string msg = err ? err : "Unknown error";
        if (err) sqlite3_free(err);
        LOG_ERROR("SQL error: {} | Query: {}", msg, sql);
        throw CDbError("SQL error: " + msg);
    }
}

std::unique_ptr<ISqlStatement> CSqliteDatabase::Prepare(std::string_view sql)
{
    LOG_DEBUG("Preparing SQL: {}", sql);
    sqlite3_stmt* raw = nullptr;
    if (sqlite3_prepare_v2(m_db.get(), sql.data(), static_cast<int>(sql.size()), &raw, nullptr) != SQLITE_OK)
    {
        std::string msg = sqlite3_errmsg(m_db.get());
        throw CDbError("Failed to prepare statement: " + msg + " | Query: " + std::string(sql));
    }
    return std::make_unique<CSqliteStatement>(raw);
}

void CSqliteDatabase::BeginTransaction()
{
    Exec(Sql::BeginTransaction);
}

void CSqliteDatabase::Commit()
{
    Exec(Sql::CommitTransaction);
}

void CSqliteDatabase::Rollback()
{
    Exec(Sql::RollbackTransaction);
}

int64_t CSqliteDatabase::LastInsertRowId() const
{
    return sqlite3_last_insert_rowid(m_db.get());
}

} // namespace Librium::Db
