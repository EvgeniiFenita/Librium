#include "DatabaseSchema.hpp"
#include "SqlQueries.hpp"
#include "Log/Logger.hpp"
#include "SqlDatabase.hpp"
#include "Database.hpp"

namespace Librium::Db {

void CDatabaseSchema::Create(ISqlDatabase& db)
{
    LOG_INFO("Initializing database schema...");

    // IMPORTANT: page_size MUST be the first pragma, before any writes
    Exec(db, Sql::PragmaPageSize.data());
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
    Exec(db, Sql::CreateIndexBooksSearchTitle.data());
    Exec(db, Sql::CreateIndexAuthorsSearchName.data());
    
    LOG_INFO("Database schema is ready.");
}

void CDatabaseSchema::Exec(ISqlDatabase& db, const char* sql)
{
    try
    {
        db.Exec(sql);
    }
    catch (const CDbError& e)
    {
        LOG_ERROR("Schema creation error: {} | Query: {}", e.what(), sql);
    }
}

} // namespace Librium::Db
