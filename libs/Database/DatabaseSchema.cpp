#include "DatabaseSchema.hpp"
#include "SqlQueries.hpp"
#include "Log/Logger.hpp"

#include <sqlite3.h>

namespace Librium::Db {

void CDatabaseSchema::Create(sqlite3* db)
{
    LOG_INFO("Initializing database schema...");

    Exec(db, Sql::PragmaWal.data());
    Exec(db, Sql::PragmaSyncNormal.data());

    Exec(db, Sql::CreateTableArchives.data());
    Exec(db, Sql::CreateTableAuthors.data());
    Exec(db, Sql::CreateTableGenres.data());
    Exec(db, Sql::CreateTableSeries.data());
    Exec(db, Sql::CreateTablePublishers.data());
    Exec(db, Sql::CreateTableBooks.data());
    Exec(db, Sql::CreateTableBookAuthors.data());
    Exec(db, Sql::CreateTableBookGenres.data());

    Exec(db, Sql::CreateIndexBooksTitle.data());
    Exec(db, Sql::CreateIndexBooksLang.data());
    
    LOG_INFO("Database schema is ready.");
}

void CDatabaseSchema::Exec(sqlite3* db, const char* sql)
{
    char* err = nullptr;
    if (sqlite3_exec(db, sql, nullptr, nullptr, &err) != SQLITE_OK)
    {
        std::string msg = err;
        sqlite3_free(err);
        LOG_ERROR("Schema creation error: {} | Query: {}", msg, sql);
    }
}

} // namespace Librium::Db
