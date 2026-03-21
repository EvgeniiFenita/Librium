#pragma once

#include "SqlDatabase.hpp"
#include <string>

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

    void Exec(const std::string& sql) override;
    [[nodiscard]] std::unique_ptr<ISqlStatement> Prepare(const std::string& sql) override;

    void BeginTransaction() override;
    void Commit() override;
    void Rollback() override;

    [[nodiscard]] int64_t LastInsertRowId() const override;

private:
    sqlite3* m_db;
};

} // namespace Librium::Db
