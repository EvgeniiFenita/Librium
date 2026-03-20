#pragma once

#include "Fb2/Fb2Parser.hpp"
#include "Inpx/BookRecord.hpp"
#include "Log/Logger.hpp"
#include "Config/AppConfig.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>
#include <unordered_map>

struct sqlite3;
struct sqlite3_stmt;

namespace Librium::Db {

// RAII deleters for SQLite resources
struct SSqliteDeleter 
{
    void operator()(sqlite3* db) const;
    void operator()(sqlite3_stmt* stmt) const;
};

using sqlite3_ptr      = std::unique_ptr<sqlite3, SSqliteDeleter>;
using sqlite3_stmt_ptr = std::unique_ptr<sqlite3_stmt, SSqliteDeleter>;

struct SBookPath
{
    std::string archiveName;
    std::string fileName;
};

class CDbError : public std::runtime_error
{
public:
    explicit CDbError(const std::string& msg) : std::runtime_error(msg) 
    {}
};

struct SImportStats
{
    size_t booksInserted{0};
    size_t booksUpdated{0};
    size_t booksSkipped{0};
    size_t booksFiltered{0};
    size_t fb2Parsed{0};
    size_t fb2Errors{0};
    size_t archivesProcessed{0};

    void PrintSummary() const
    {
        LOG_INFO("=== Import Summary ===");
        LOG_INFO("  Archives processed : {}", archivesProcessed);
        LOG_INFO("  Books inserted     : {}", booksInserted);
        LOG_INFO("  Books skipped      : {}", booksSkipped);
        LOG_INFO("  Books filtered     : {}", booksFiltered);
        LOG_INFO("  FB2 parsed         : {}", fb2Parsed);
        LOG_INFO("  FB2 errors         : {}", fb2Errors);
        LOG_INFO("======================");
    }
};

class CDatabase
{
public:
    explicit CDatabase(const std::string& path, const Config::SImportConfig& cfg = Config::SImportConfig{});
    ~CDatabase() = default;

    CDatabase(const CDatabase&)            = delete;
    CDatabase& operator=(const CDatabase&) = delete;

    void BeginTransaction();
    void Commit();
    void Rollback();

    [[nodiscard]] int64_t InsertBook(
        const Inpx::SBookRecord& record,
        const Fb2::SFb2Data&     fb2 = {});

    [[nodiscard]] bool BookExists(
        const std::string& libId,
        const std::string& archiveName);

    [[nodiscard]] std::vector<std::string> GetIndexedArchives();
    void MarkArchiveIndexed(const std::string& archiveName);

    [[nodiscard]] std::optional<SBookPath> GetBookPath(int64_t bookId);

    [[nodiscard]] int64_t CountBooks() const;
    [[nodiscard]] int64_t CountAuthors() const;

    sqlite3* Handle() 
    { 
        return m_db.get(); 
    }

    void DropIndexes();
    void CreateIndexes();
    void Exec(const char* sql);

private:
    sqlite3_ptr m_db;

    sqlite3_stmt_ptr m_stmtInsertBook;
    sqlite3_stmt_ptr m_stmtInsertAuthor;
    sqlite3_stmt_ptr m_stmtGetAuthor;
    sqlite3_stmt_ptr m_stmtInsertGenre;
    sqlite3_stmt_ptr m_stmtGetGenre;
    sqlite3_stmt_ptr m_stmtInsertSeries;
    sqlite3_stmt_ptr m_stmtGetSeries;
    sqlite3_stmt_ptr m_stmtInsertArchive;
    sqlite3_stmt_ptr m_stmtGetArchive;
    sqlite3_stmt_ptr m_stmtInsertBookAuthor;
    sqlite3_stmt_ptr m_stmtInsertBookGenre;
    sqlite3_stmt_ptr m_stmtBookExists;
    sqlite3_stmt_ptr m_stmtUpdateFb2;
    sqlite3_stmt_ptr m_stmtGetBookPath;
    sqlite3_stmt_ptr m_stmtInsertPublisher;
    sqlite3_stmt_ptr m_stmtGetPublisher;

    // Memory caches for bulk indexing speed
    std::unordered_map<std::string, int64_t> m_cacheArchives;
    std::unordered_map<std::string, int64_t> m_cacheGenres;
    std::unordered_map<std::string, int64_t> m_cacheSeries;
    std::unordered_map<std::string, int64_t> m_cachePublishers;
    std::unordered_map<std::string, int64_t> m_cacheAuthors;

    void PrepareStatements();

    [[nodiscard]] int64_t GetOrCreateAuthor(const Inpx::SAuthor& author);
    [[nodiscard]] int64_t GetOrCreateGenre(const std::string& genre);
    [[nodiscard]] int64_t GetOrCreateSeries(const std::string& series);
    [[nodiscard]] int64_t GetOrCreatePublisher(const std::string& publisher);
    [[nodiscard]] int64_t GetOrCreateArchive(const std::string& archiveName);

    [[nodiscard]] int64_t LastInsertRowId() const;
    static void Check(int rc, const char* context);
};

} // namespace Librium::Db
