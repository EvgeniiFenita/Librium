#include "Database.hpp"

#include "SqlQueries.hpp"

namespace Librium::Db {

std::vector<std::string> CDatabase::GetIndexedArchives()
{
    std::vector<std::string> result;
    auto stmt = m_db->Prepare(std::string(Sql::GetIndexedArchives));

    stmt->Step();
    CSqlStmtResetGuard guard(*stmt);
    while (stmt->IsRow())
    {
        std::string text = stmt->ColumnText(0);
        if (!text.empty())
        {
            result.emplace_back(text);
        }
        stmt->Step();
    }
    return result;
}

void CDatabase::MarkArchiveIndexed(const std::string& archiveName)
{
    (void)GetOrCreateArchive(archiveName);
    auto stmt = m_db->Prepare(std::string(Sql::UpdateArchiveIndexed));

    stmt->BindText(1, archiveName);
    stmt->Step();
    CSqlStmtResetGuard guard(*stmt);
}

std::optional<SBookPath> CDatabase::GetBookPath(int64_t bookId)
{
    m_stmtGetBookPath->Reset();
    m_stmtGetBookPath->BindInt64(1, bookId);
    m_stmtGetBookPath->Step();
    CSqlStmtResetGuard guard(*m_stmtGetBookPath);

    if (m_stmtGetBookPath->IsRow())
    {
        SBookPath bookPath;
        bookPath.archiveName = m_stmtGetBookPath->ColumnText(0);
        bookPath.fileName    = m_stmtGetBookPath->ColumnText(1);
        return bookPath;
    }
    return std::nullopt;
}

} // namespace Librium::Db
