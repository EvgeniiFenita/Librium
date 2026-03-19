#pragma once

#include "Fb2/Fb2Parser.hpp"
#include "Inpx/BookRecord.hpp"
#include "Log/Logger.hpp"

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>
#include <unordered_map>

struct sqlite3;
struct sqlite3_stmt;

namespace Librium::Db {

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
    explicit CDatabase(const std::string& path);
    ~CDatabase();

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
        return m_db; 
    }

    void DropIndexes();
    void CreateIndexes();
    void Exec(const char* sql);

private:
    sqlite3* m_db{nullptr};

    sqlite3_stmt* m_stmtInsertBook{nullptr};
    sqlite3_stmt* m_stmtInsertAuthor{nullptr};
    sqlite3_stmt* m_stmtGetAuthor{nullptr};
    sqlite3_stmt* m_stmtInsertGenre{nullptr};
    sqlite3_stmt* m_stmtGetGenre{nullptr};
    sqlite3_stmt* m_stmtInsertSeries{nullptr};
    sqlite3_stmt* m_stmtGetSeries{nullptr};
    sqlite3_stmt* m_stmtInsertArchive{nullptr};
    sqlite3_stmt* m_stmtGetArchive{nullptr};
    sqlite3_stmt* m_stmtInsertBookAuthor{nullptr};
    sqlite3_stmt* m_stmtInsertBookGenre{nullptr};
    sqlite3_stmt* m_stmtBookExists{nullptr};
    sqlite3_stmt* m_stmtUpdateFb2{nullptr};
    sqlite3_stmt* m_stmtGetBookPath{nullptr};
    sqlite3_stmt* m_stmtInsertPublisher{nullptr};
    sqlite3_stmt* m_stmtGetPublisher{nullptr};

    // Memory caches for bulk indexing speed
    std::unordered_map<std::string, int64_t> m_cacheArchives;
    std::unordered_map<std::string, int64_t> m_cacheGenres;
    std::unordered_map<std::string, int64_t> m_cacheSeries;
    std::unordered_map<std::string, int64_t> m_cachePublishers;
    std::unordered_map<std::string, int64_t> m_cacheAuthors;

    void PrepareStatements();
    void FinalizeStatements();

    [[nodiscard]] int64_t GetOrCreateAuthor(const Inpx::SAuthor& author);
    [[nodiscard]] int64_t GetOrCreateGenre(const std::string& genre);
    [[nodiscard]] int64_t GetOrCreateSeries(const std::string& series);
    [[nodiscard]] int64_t GetOrCreatePublisher(const std::string& publisher);
    [[nodiscard]] int64_t GetOrCreateArchive(const std::string& archiveName);

    [[nodiscard]] int64_t LastInsertRowId() const;
    static void Check(int rc, const char* context);
};

} // namespace Librium::Db
