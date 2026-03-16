#pragma once

#include "Fb2/Fb2Parser.hpp"
#include "Inpx/BookRecord.hpp"

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

struct sqlite3;
struct sqlite3_stmt;

namespace Librium::Db {

class CDbError : public std::runtime_error
{
public:
    explicit CDbError(const std::string& msg) : std::runtime_error(msg) 
{}
};

struct CImportStats
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
        std::cout << "\n=== Import Summary ===\n"
                  << "  Archives processed : " << archivesProcessed << "\n"
                  << "  Books inserted     : " << booksInserted     << "\n"
                  << "  Books skipped      : " << booksSkipped      << "\n"
                  << "  Books filtered     : " << booksFiltered     << "\n"
                  << "  FB2 parsed         : " << fb2Parsed         << "\n"
                  << "  FB2 errors         : " << fb2Errors         << "\n"
                  << "======================\n";
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
        const Inpx::CBookRecord& record,
        const Fb2::CFb2Data&     fb2 = {});

    [[nodiscard]] bool BookExists(
        const std::string& libId,
        const std::string& archiveName);

    void UpdateBookFb2(int64_t bookId, const Fb2::CFb2Data& fb2);

    [[nodiscard]] std::vector<std::string> GetIndexedArchives();
    void MarkArchiveIndexed(const std::string& archiveName);
    [[nodiscard]] bool ArchiveExists(const std::string& archiveName);

    [[nodiscard]] int64_t CountBooks() const;
    [[nodiscard]] int64_t CountAuthors() const;

    sqlite3* Handle() 
{ return m_db; }

private:
    sqlite3* m_db{nullptr};

    sqlite3_stmt* m_stmtInsertBook{nullptr};
    sqlite3_stmt* m_stmtInsertAuthor{nullptr};
    sqlite3_stmt* m_stmtGetAuthor{nullptr};
    sqlite3_stmt* m_stmtInsertGenre{nullptr};
    sqlite3_stmt* m_stmtGetGenre{nullptr};
    sqlite3_stmt* m_stmtInsertArchive{nullptr};
    sqlite3_stmt* m_stmtGetArchive{nullptr};
    sqlite3_stmt* m_stmtInsertBookAuthor{nullptr};
    sqlite3_stmt* m_stmtInsertBookGenre{nullptr};
    sqlite3_stmt* m_stmtBookExists{nullptr};
    sqlite3_stmt* m_stmtUpdateFb2{nullptr};

    void CreateSchema();
    void PrepareStatements();
    void FinalizeStatements();

    [[nodiscard]] int64_t GetOrCreateAuthor(const Inpx::CAuthor& CAuthor);
    [[nodiscard]] int64_t GetOrCreateGenre(const std::string& genre);
    [[nodiscard]] int64_t GetOrCreateSeries(const std::string& series);
    [[nodiscard]] int64_t GetOrCreatePublisher(const std::string& publisher);
    [[nodiscard]] int64_t GetOrCreateArchive(const std::string& archiveName);

    void Exec(const char* sql);
    [[nodiscard]] int64_t LastInsertRowId() const;
    static void Check(int rc, const char* context);
};

} // namespace Librium::Db






