#include "Database.hpp"

#include "SqlQueries.hpp"

namespace Librium::Db {

int64_t CDatabase::CountBooks() const
{
    auto stmt = m_db->Prepare(std::string(Sql::CountBooks));
    stmt->Step();
    CSqlStmtResetGuard guard(*stmt);
    return stmt->IsRow() ? stmt->ColumnInt64(0) : 0;
}

int64_t CDatabase::CountAuthors() const
{
    auto stmt = m_db->Prepare(std::string(Sql::CountAuthors));
    stmt->Step();
    CSqlStmtResetGuard guard(*stmt);
    return stmt->IsRow() ? stmt->ColumnInt64(0) : 0;
}

int64_t CDatabase::CountIndexedArchives() const
{
    auto stmt = m_db->Prepare(std::string(Sql::CountIndexedArchives));
    stmt->Step();
    CSqlStmtResetGuard guard(*stmt);
    return stmt->IsRow() ? stmt->ColumnInt64(0) : 0;
}

int CDatabase::CountIndexes() const
{
    auto stmt = m_db->Prepare(std::string(Sql::CountBookIndexes));
    stmt->Step();
    CSqlStmtResetGuard guard(*stmt);
    return stmt->IsRow() ? stmt->ColumnInt(0) : 0;
}

} // namespace Librium::Db
