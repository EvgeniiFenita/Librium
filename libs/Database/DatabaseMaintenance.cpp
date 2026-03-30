#include "Database.hpp"

#include "SqlQueries.hpp"

namespace Librium::Db {

void CDatabase::DropIndexes()
{
    LOG_INFO("Dropping indexes for bulk import...");
    Exec(std::string(Sql::DropIndexBooksTitle));
    Exec(std::string(Sql::DropIndexBooksLang));
    Exec(std::string(Sql::DropIndexBooksSearchTitle));
    Exec(std::string(Sql::DropIndexAuthorsSearchName));
    Exec(std::string(Sql::DropIndexSeriesSearchName));
}

void CDatabase::CreateIndexes()
{
    LOG_INFO("Re-creating indexes after import...");
    Exec(std::string(Sql::CreateIndexBooksTitle));
    Exec(std::string(Sql::CreateIndexBooksLang));
    Exec(std::string(Sql::CreateIndexBooksSearchTitle));
    Exec(std::string(Sql::CreateIndexAuthorsSearchName));
    Exec(std::string(Sql::CreateIndexSeriesSearchName));
}

} // namespace Librium::Db
