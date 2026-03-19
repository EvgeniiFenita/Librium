#include "Database.hpp"
#include "DatabaseSchema.hpp"
#include "SqlQueries.hpp"
#include "Log/Logger.hpp"

#include <sqlite3.h>

#include <filesystem>

namespace Librium::Db {

CDatabase::CDatabase(const std::string& path)
{
    LOG_INFO("Opening database: {}", path);
    if (sqlite3_open(path.c_str(), &m_db) != SQLITE_OK)
        throw CDbError("Cannot open database: " + path);

    CDatabaseSchema::Create(m_db);
    PrepareStatements();
}

CDatabase::~CDatabase()
{
    FinalizeStatements();
    sqlite3_close(m_db);
}

void CDatabase::BeginTransaction()
{
    Exec("BEGIN TRANSACTION");
}

void CDatabase::Commit()
{
    Exec("COMMIT");
}

void CDatabase::Rollback()
{
    Exec("ROLLBACK");
}

void CDatabase::Exec(const char* sql)
{
    LOG_DEBUG("Executing SQL: {}", sql);
    char* err = nullptr;
    if (sqlite3_exec(m_db, sql, nullptr, nullptr, &err) != SQLITE_OK)
    {
        std::string msg = err;
        sqlite3_free(err);
        LOG_ERROR("SQL error: {} | Query: {}", msg, sql);
        throw CDbError("SQL error: " + msg);
    }
}

void CDatabase::PrepareStatements()
{
    auto prep = [&](std::string_view sql, sqlite3_stmt** stmt)
    {
        Check(sqlite3_prepare_v2(m_db, sql.data(), -1, stmt, nullptr), "prepare");
    };

    prep(Sql::InsertAuthor, &m_stmtInsertAuthor);
    prep(Sql::GetAuthorId, &m_stmtGetAuthor);
    prep(Sql::InsertGenre, &m_stmtInsertGenre);
    prep(Sql::GetGenreId, &m_stmtGetGenre);
    prep(Sql::InsertSeries, &m_stmtInsertSeries);
    prep(Sql::GetSeriesId, &m_stmtGetSeries);
    prep(Sql::InsertArchive, &m_stmtInsertArchive);
    prep(Sql::GetArchiveId, &m_stmtGetArchive);
    prep(Sql::InsertBook, &m_stmtInsertBook);
    prep(Sql::InsertBookAuthor, &m_stmtInsertBookAuthor);
    prep(Sql::InsertBookGenre, &m_stmtInsertBookGenre);
    prep(Sql::CheckBookExists, &m_stmtBookExists);
    prep(Sql::UpdateBookFb2, &m_stmtUpdateFb2);
    prep(Sql::GetBookPath, &m_stmtGetBookPath);
}

void CDatabase::FinalizeStatements()
{
    sqlite3_finalize(m_stmtInsertBook);
    sqlite3_finalize(m_stmtInsertAuthor);
    sqlite3_finalize(m_stmtGetAuthor);
    sqlite3_finalize(m_stmtInsertGenre);
    sqlite3_finalize(m_stmtGetGenre);
    sqlite3_finalize(m_stmtInsertSeries);
    sqlite3_finalize(m_stmtGetSeries);
    sqlite3_finalize(m_stmtInsertArchive);
    sqlite3_finalize(m_stmtGetArchive);
    sqlite3_finalize(m_stmtInsertBookAuthor);
    sqlite3_finalize(m_stmtInsertBookGenre);
    sqlite3_finalize(m_stmtBookExists);
    sqlite3_finalize(m_stmtUpdateFb2);
    sqlite3_finalize(m_stmtGetBookPath);
}

int64_t CDatabase::GetOrCreateAuthor(const Inpx::SAuthor& author)
{
    sqlite3_reset(m_stmtInsertAuthor);
    sqlite3_bind_text(m_stmtInsertAuthor, 1, author.lastName.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(m_stmtInsertAuthor, 2, author.firstName.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(m_stmtInsertAuthor, 3, author.middleName.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(m_stmtInsertAuthor) != SQLITE_DONE)
    {
        LOG_ERROR("Failed to insert author");
        throw CDbError("Failed to insert author");
    }

    sqlite3_reset(m_stmtGetAuthor);
    sqlite3_bind_text(m_stmtGetAuthor, 1, author.lastName.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(m_stmtGetAuthor, 2, author.firstName.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(m_stmtGetAuthor, 3, author.middleName.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(m_stmtGetAuthor) == SQLITE_ROW)
        return sqlite3_column_int64(m_stmtGetAuthor, 0);
    return 0;
}

int64_t CDatabase::GetOrCreateGenre(const std::string& genre)
{
    sqlite3_reset(m_stmtInsertGenre);
    sqlite3_bind_text(m_stmtInsertGenre, 1, genre.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(m_stmtInsertGenre) != SQLITE_DONE)
    {
        LOG_ERROR("Failed to insert genre: {}", genre);
        throw CDbError("Failed to insert genre");
    }

    sqlite3_reset(m_stmtGetGenre);
    sqlite3_bind_text(m_stmtGetGenre, 1, genre.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(m_stmtGetGenre) == SQLITE_ROW)
        return sqlite3_column_int64(m_stmtGetGenre, 0);
    return 0;
}

int64_t CDatabase::GetOrCreateSeries(const std::string& series)
{
    if (series.empty()) return 0;
    sqlite3_reset(m_stmtInsertSeries);
    sqlite3_bind_text(m_stmtInsertSeries, 1, series.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(m_stmtInsertSeries) != SQLITE_DONE)
    {
        LOG_ERROR("Failed to insert series: {}", series);
        throw CDbError("Failed to insert series");
    }

    sqlite3_reset(m_stmtGetSeries);
    sqlite3_bind_text(m_stmtGetSeries, 1, series.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(m_stmtGetSeries) == SQLITE_ROW)
        return sqlite3_column_int64(m_stmtGetSeries, 0);
    return 0;
}

int64_t CDatabase::GetOrCreateArchive(const std::string& name)
{
    sqlite3_reset(m_stmtInsertArchive);
    sqlite3_bind_text(m_stmtInsertArchive, 1, name.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(m_stmtInsertArchive) != SQLITE_DONE)
    {
        LOG_ERROR("Failed to insert archive: {}", name);
        throw CDbError("Failed to insert archive");
    }

    sqlite3_reset(m_stmtGetArchive);
    sqlite3_bind_text(m_stmtGetArchive, 1, name.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(m_stmtGetArchive) == SQLITE_ROW)
        return sqlite3_column_int64(m_stmtGetArchive, 0);
    return 0;
}

int64_t CDatabase::InsertBook(const Inpx::SBookRecord& rec, const Fb2::SFb2Data& fb2)
{
    int64_t archId = GetOrCreateArchive(rec.archiveName);
    int64_t seriesId = GetOrCreateSeries(rec.series);

    sqlite3_reset(m_stmtInsertBook);
    sqlite3_bind_text(m_stmtInsertBook, 1, rec.libId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(m_stmtInsertBook, 2, archId);
    sqlite3_bind_text(m_stmtInsertBook, 3, rec.title.c_str(), -1, SQLITE_TRANSIENT);
    
    // Series
    if (seriesId > 0)
        sqlite3_bind_int64(m_stmtInsertBook, 4, seriesId);
    else
        sqlite3_bind_null(m_stmtInsertBook, 4);

    sqlite3_bind_int(m_stmtInsertBook, 5, rec.seriesNumber);

    sqlite3_bind_text(m_stmtInsertBook, 6, rec.fileName.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(m_stmtInsertBook, 7, rec.fileSize);
    sqlite3_bind_text(m_stmtInsertBook, 8, rec.fileExt.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(m_stmtInsertBook, 9, rec.dateAdded.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(m_stmtInsertBook, 10, rec.language.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(m_stmtInsertBook, 11, rec.rating);
    sqlite3_bind_text(m_stmtInsertBook, 12, rec.keywords.c_str(), -1, SQLITE_TRANSIENT);

    // FB2 Metadata
    sqlite3_bind_text(m_stmtInsertBook, 13, fb2.annotation.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_null(m_stmtInsertBook, 14); // publisher_id
    sqlite3_bind_text(m_stmtInsertBook, 15, fb2.isbn.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(m_stmtInsertBook, 16, fb2.publishYear.c_str(), -1, SQLITE_TRANSIENT);
    
    // Cover data was here (removed)

    if (sqlite3_step(m_stmtInsertBook) != SQLITE_DONE)
        return 0;

    int64_t bookId = LastInsertRowId();

    for (const auto& author : rec.authors)
    {
        int64_t aid = GetOrCreateAuthor(author);
        sqlite3_reset(m_stmtInsertBookAuthor);
        sqlite3_bind_int64(m_stmtInsertBookAuthor, 1, bookId);
        sqlite3_bind_int64(m_stmtInsertBookAuthor, 2, aid);
        if (sqlite3_step(m_stmtInsertBookAuthor) != SQLITE_DONE)
        {
            LOG_ERROR("Failed to insert book_author link");
            throw CDbError("Failed to insert book_author link");
        }
    }

    for (const auto& genre : rec.genres)
    {
        int64_t gid = GetOrCreateGenre(genre);
        sqlite3_reset(m_stmtInsertBookGenre);
        sqlite3_bind_int64(m_stmtInsertBookGenre, 1, bookId);
        sqlite3_bind_int64(m_stmtInsertBookGenre, 2, gid);
        if (sqlite3_step(m_stmtInsertBookGenre) != SQLITE_DONE)
        {
            LOG_ERROR("Failed to insert book_genre link");
            throw CDbError("Failed to insert book_genre link");
        }
    }

    return bookId;
}

bool CDatabase::BookExists(const std::string& libId, const std::string& archiveName)
{
    sqlite3_reset(m_stmtGetArchive);
    sqlite3_bind_text(m_stmtGetArchive, 1, archiveName.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(m_stmtGetArchive) != SQLITE_ROW)
        return false;
        
    int64_t archId = sqlite3_column_int64(m_stmtGetArchive, 0);
    sqlite3_reset(m_stmtBookExists);
    sqlite3_bind_text(m_stmtBookExists, 1, libId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(m_stmtBookExists, 2, archId);
    return sqlite3_step(m_stmtBookExists) == SQLITE_ROW;
}

std::vector<std::string> CDatabase::GetIndexedArchives()
{
    std::vector<std::string> res;
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(m_db, Sql::GetIndexedArchives.data(), -1, &stmt, nullptr);
    while (sqlite3_step(stmt) == SQLITE_ROW)
        res.push_back(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)));
    sqlite3_finalize(stmt);
    return res;
}

void CDatabase::MarkArchiveIndexed(const std::string& archiveName)
{
    (void)GetOrCreateArchive(archiveName);
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(m_db, Sql::UpdateArchiveIndexed.data(), -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, archiveName.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

int64_t CDatabase::CountBooks() const
{
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(m_db, Sql::CountBooks.data(), -1, &stmt, nullptr);
    sqlite3_step(stmt);
    int64_t res = sqlite3_column_int64(stmt, 0);
    sqlite3_finalize(stmt);
    return res;
}

int64_t CDatabase::CountAuthors() const
{
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(m_db, Sql::CountAuthors.data(), -1, &stmt, nullptr);
    sqlite3_step(stmt);
    int64_t res = sqlite3_column_int64(stmt, 0);
    sqlite3_finalize(stmt);
    return res;
}

int64_t CDatabase::LastInsertRowId() const
{
    return sqlite3_last_insert_rowid(m_db);
}

void CDatabase::Check(int rc, const char* context)
{
    if (rc != SQLITE_OK && rc != SQLITE_DONE && rc != SQLITE_ROW)
        throw CDbError(std::string(context) + ": " + std::to_string(rc));
}

std::optional<SBookPath> CDatabase::GetBookPath(int64_t bookId)
{
    sqlite3_reset(m_stmtGetBookPath);
    sqlite3_bind_int64(m_stmtGetBookPath, 1, bookId);
    
    if (sqlite3_step(m_stmtGetBookPath) == SQLITE_ROW)
    {
        SBookPath bp;
        bp.archiveName = reinterpret_cast<const char*>(sqlite3_column_text(m_stmtGetBookPath, 0));
        bp.fileName = reinterpret_cast<const char*>(sqlite3_column_text(m_stmtGetBookPath, 1));
        return bp;
    }
    return std::nullopt;
}

} // namespace Librium::Db
