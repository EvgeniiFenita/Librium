#include "SqliteStatement.hpp"
#include "Database.hpp" // For CDbError
#include <sqlite3.h>

namespace Librium::Db {

namespace {

void FinalizeStatement(sqlite3_stmt* statement)
{
    if (statement)
    {
        sqlite3_finalize(statement);
    }
}

void CheckSqliteResult(int rc, const char* context)
{
    if (rc != SQLITE_OK && rc != SQLITE_DONE && rc != SQLITE_ROW)
    {
        throw CDbError(std::string(context) + ": " + sqlite3_errstr(rc) + " (code " + std::to_string(rc) + ")");
    }
}

} // namespace

CSqliteStatement::CSqliteStatement(sqlite3_stmt* stmt)
    : m_stmt(stmt, FinalizeStatement), m_lastRc(SQLITE_OK)
{
    if (!m_stmt)
    {
        throw CDbError("Invalid sqlite3_stmt handle");
    }
}

CSqliteStatement::~CSqliteStatement() = default;

void CSqliteStatement::Check(int rc, const char* context) const
{
    CheckSqliteResult(rc, context);
}

void CSqliteStatement::BindInt(int index, int value)
{
    Check(sqlite3_bind_int(m_stmt.get(), index, value), "BindInt");
}

void CSqliteStatement::BindInt64(int index, int64_t value)
{
    Check(sqlite3_bind_int64(m_stmt.get(), index, value), "BindInt64");
}

void CSqliteStatement::BindText(int index, std::string_view value)
{
    Check(sqlite3_bind_text(m_stmt.get(), index, value.data(), static_cast<int>(value.size()), SQLITE_TRANSIENT), "BindText");
}

void CSqliteStatement::BindNull(int index)
{
    Check(sqlite3_bind_null(m_stmt.get(), index), "BindNull");
}

void CSqliteStatement::Step()
{
    m_lastRc = sqlite3_step(m_stmt.get());
    Check(m_lastRc, "Step");
}

void CSqliteStatement::Reset()
{
    Check(sqlite3_reset(m_stmt.get()), "Reset");
    m_lastRc = SQLITE_OK; // reset status
}

int CSqliteStatement::ColumnInt(int index) const
{
    return sqlite3_column_int(m_stmt.get(), index);
}

int64_t CSqliteStatement::ColumnInt64(int index) const
{
    return sqlite3_column_int64(m_stmt.get(), index);
}

std::string CSqliteStatement::ColumnText(int index) const
{
    const auto* text = reinterpret_cast<const char*>(sqlite3_column_text(m_stmt.get(), index));
    if (text)
    {
        return std::string(text);
    }
    return "";
}

bool CSqliteStatement::IsRow() const
{
    return m_lastRc == SQLITE_ROW;
}

bool CSqliteStatement::IsDone() const
{
    return m_lastRc == SQLITE_DONE;
}

} // namespace Librium::Db
