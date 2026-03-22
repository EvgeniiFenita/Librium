#pragma once

#include "SqlStatement.hpp"
#include <string>
#include <memory>

struct sqlite3_stmt;

namespace Librium::Db {

class CSqliteStatement : public ISqlStatement
{
public:
    explicit CSqliteStatement(sqlite3_stmt* stmt);
    ~CSqliteStatement() override;

    // Disallow copy/move
    CSqliteStatement(const CSqliteStatement&) = delete;
    CSqliteStatement& operator=(const CSqliteStatement&) = delete;

    void BindInt(int index, int value) override;
    void BindInt64(int index, int64_t value) override;
    void BindText(int index, const std::string& value) override;
    void BindNull(int index) override;

    void Step() override;
    void Reset() override;

    [[nodiscard]] int ColumnInt(int index) const override;
    [[nodiscard]] int64_t ColumnInt64(int index) const override;
    [[nodiscard]] std::string ColumnText(int index) const override;

    [[nodiscard]] bool IsRow() const override;
    [[nodiscard]] bool IsDone() const override;

private:
    using StmtPtr = std::unique_ptr<sqlite3_stmt, void(*)(sqlite3_stmt*)>;
    StmtPtr m_stmt;
    int m_lastRc;

    void Check(int rc, const char* context) const;
};

} // namespace Librium::Db
