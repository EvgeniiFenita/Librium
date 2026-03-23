#pragma once

#include "Fb2/Fb2Parser.hpp"
#include "Inpx/BookRecord.hpp"
#include "Log/Logger.hpp"
#include "Config/AppConfig.hpp"
#include "SqlDatabase.hpp"
#include "SqlStatement.hpp"
#include "QueryTypes.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>
#include <unordered_map>

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
    double totalTimeMs{0};

    void PrintSummary() const
    {
        double seconds = totalTimeMs / 1000.0;
        double speed = seconds > 0 ? (booksInserted + booksUpdated) / seconds : 0;

        LOG_INFO("=== Import Summary ===");
        LOG_INFO("  Archives processed : {}", archivesProcessed);
        LOG_INFO("  Books handled      : {}", booksInserted + booksUpdated);
        LOG_INFO("  Books skipped      : {}", booksSkipped);
        LOG_INFO("  Books filtered     : {}", booksFiltered);
        LOG_INFO("  FB2 parsed         : {}", fb2Parsed);
        LOG_INFO("  FB2 errors         : {}", fb2Errors);
        LOG_INFO("  Total time         : {:.2f}s", seconds);
        LOG_INFO("  Average speed      : {:.0f} books/sec", speed);
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

    [[nodiscard]] SQueryResult ExecuteQuery(const SQueryParams& params);
    [[nodiscard]] std::optional<SBookResult> GetBookById(int64_t id);

    ISqlDatabase* Handle()
    {
        return m_db.get();
    }

    void DropIndexes();
    void CreateIndexes();
    void BeginBulkImport();
    void EndBulkImport();
    void Exec(const std::string& sql);

private:
    std::unique_ptr<ISqlDatabase> m_db;

    std::unique_ptr<ISqlStatement> m_stmtInsertBook;
    std::unique_ptr<ISqlStatement> m_stmtInsertAuthor;
    std::unique_ptr<ISqlStatement> m_stmtGetAuthor;
    std::unique_ptr<ISqlStatement> m_stmtInsertGenre;
    std::unique_ptr<ISqlStatement> m_stmtGetGenre;
    std::unique_ptr<ISqlStatement> m_stmtInsertSeries;
    std::unique_ptr<ISqlStatement> m_stmtGetSeries;
    std::unique_ptr<ISqlStatement> m_stmtInsertArchive;
    std::unique_ptr<ISqlStatement> m_stmtGetArchive;
    std::unique_ptr<ISqlStatement> m_stmtInsertBookAuthor;
    std::unique_ptr<ISqlStatement> m_stmtInsertBookGenre;
    std::unique_ptr<ISqlStatement> m_stmtBookExists;
    std::unique_ptr<ISqlStatement> m_stmtUpdateFb2;
    std::unique_ptr<ISqlStatement> m_stmtGetBookPath;
    std::unique_ptr<ISqlStatement> m_stmtInsertPublisher;
    std::unique_ptr<ISqlStatement> m_stmtGetPublisher;

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
};

} // namespace Librium::Db
