#include "SqliteDatabase.hpp"
#include "SqliteStatement.hpp"
#include "SqliteFunctions.hpp"
#include "Database.hpp" // For CDbError
#include "SqlQueries.hpp"
#include "Log/Logger.hpp"

#include <sqlite3.h>

namespace Librium::Db {

namespace {

void CloseDatabase(sqlite3* db)
{
    if (db)
    {
        sqlite3_close(db);
    }
}

[[nodiscard]] std::string BuildSqlErrorMessage(const std::string& prefix, const std::string& detail, std::string_view sql = {})
{
    std::string message = prefix + detail;
    if (!sql.empty())
    {
        message += " | Query: " + std::string(sql);
    }

    return message;
}

sqlite3* OpenDatabaseOrThrow(const std::string& path)
{
    sqlite3* rawDb = nullptr;
    if (sqlite3_open(path.c_str(), &rawDb) != SQLITE_OK)
    {
        const std::string error = rawDb ? sqlite3_errmsg(rawDb) : "Unknown error";
        CloseDatabase(rawDb);
        throw CDbError("Cannot open database: " + path + " - " + error);
    }

    return rawDb;
}

void ThrowExecError(char* error, std::string_view sql)
{
    const std::string message = error ? error : "Unknown error";
    if (error)
    {
        sqlite3_free(error);
    }

    LOG_ERROR("SQL error: {} | Query: {}", message, sql);
    throw CDbError(BuildSqlErrorMessage("SQL error: ", message));
}

sqlite3_stmt* PrepareStatementOrThrow(sqlite3* db, std::string_view sql)
{
    sqlite3_stmt* rawStatement = nullptr;
    if (sqlite3_prepare_v2(db, sql.data(), static_cast<int>(sql.size()), &rawStatement, nullptr) != SQLITE_OK)
    {
        throw CDbError(BuildSqlErrorMessage("Failed to prepare statement: ", sqlite3_errmsg(db), sql));
    }

    return rawStatement;
}

} // namespace

CSqliteDatabase::CSqliteDatabase(const std::string& path, int64_t cacheSize, int64_t mmapSize)
    : m_db(nullptr, CloseDatabase)
{
    LOG_INFO("Opening SQLite database: {}", path);
    m_db.reset(OpenDatabaseOrThrow(path));

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
        ThrowExecError(err, sql);
    }
}

std::unique_ptr<ISqlStatement> CSqliteDatabase::Prepare(std::string_view sql)
{
    LOG_DEBUG("Preparing SQL: {}", sql);
    return std::make_unique<CSqliteStatement>(PrepareStatementOrThrow(m_db.get(), sql));
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
