#pragma once

#include "Log/Logger.hpp"

#include <cstdint>
#include <stdexcept>
#include <string>

namespace Librium::Db {

class ISqlStatement
{
public:
    virtual ~ISqlStatement() = default;

    virtual void BindInt(int index, int value) = 0;
    virtual void BindInt64(int index, int64_t value) = 0;
    virtual void BindText(int index, const std::string& value) = 0;
    virtual void BindNull(int index) = 0;

    virtual void Step() = 0;
    virtual void Reset() = 0;

    [[nodiscard]] virtual int ColumnInt(int index) const = 0;
    [[nodiscard]] virtual int64_t ColumnInt64(int index) const = 0;
    [[nodiscard]] virtual std::string ColumnText(int index) const = 0;

    [[nodiscard]] virtual bool IsRow() const = 0;
    [[nodiscard]] virtual bool IsDone() const = 0;
};

// RAII guard that calls Reset() on a statement when it goes out of scope.
// Use immediately after Step() to guarantee cursor release on all exit paths,
// including exceptions thrown during column reading.
class CSqlStmtResetGuard
{
public:
    explicit CSqlStmtResetGuard(ISqlStatement& stmt) noexcept : m_stmt(stmt) {}
    ~CSqlStmtResetGuard() noexcept
    {
        try
        {
            m_stmt.Reset();
        }
        catch (const std::exception& e)
        {
            // Reset errors are non-fatal and must not propagate from a destructor.
            LOG_WARN("CSqlStmtResetGuard: failed to reset statement: {}", e.what());
        }
    }
    CSqlStmtResetGuard(const CSqlStmtResetGuard&)            = delete;
    CSqlStmtResetGuard& operator=(const CSqlStmtResetGuard&) = delete;
private:
    ISqlStatement& m_stmt;
};

} // namespace Librium::Db
