#pragma once

#include "SqlDatabase.hpp"
#include <string>
#include <memory>

struct sqlite3;

namespace Librium::Db {

class CSqliteDatabase : public ISqlDatabase
{
public:
    CSqliteDatabase(const std::string& path, int64_t cacheSize, int64_t mmapSize);
    ~CSqliteDatabase() override;

    // Disallow copy/move
    CSqliteDatabase(const CSqliteDatabase&) = delete;
    CSqliteDatabase& operator=(const CSqliteDatabase&) = delete;

    void Exec(std::string_view sql) override;
    [[nodiscard]] std::unique_ptr<ISqlStatement> Prepare(std::string_view sql) override;

    void BeginTransaction() override;
    void Commit() override;
    void Rollback() override;

    [[nodiscard]] int64_t LastInsertRowId() const override;

private:
    using SqlitePtr = std::unique_ptr<sqlite3, void(*)(sqlite3*)>;
    SqlitePtr m_db;
};

} // namespace Librium::Db
