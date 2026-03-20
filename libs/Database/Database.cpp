#include "Database.hpp"
#include "DatabaseSchema.hpp"
#include "SqlQueries.hpp"
#include "Log/Logger.hpp"

#include <sqlite3.h>

#include <filesystem>
#include <sstream>

namespace Librium::Db {

void SSqliteDeleter::operator()(sqlite3* db) const
{
    if (db) sqlite3_close(db);
}

void SSqliteDeleter::operator()(sqlite3_stmt* stmt) const
{
    if (stmt) sqlite3_finalize(stmt);
}

CDatabase::CDatabase(const std::string& path, const Config::SImportConfig& cfg)
{
    LOG_INFO("Opening database: {}", path);
    sqlite3* db = nullptr;
    if (sqlite3_open(path.c_str(), &db) != SQLITE_OK)
    {
        if (db) sqlite3_close(db);
        throw CDbError("Cannot open database: " + path);
    }
    m_db.reset(db);

    // Performance optimizations for bulk loading
    Exec(("PRAGMA cache_size = " + std::to_string(cfg.sqliteCacheSize)).c_str());
    Exec("PRAGMA page_size = 4096");
    Exec("PRAGMA temp_store = MEMORY");
    Exec(("PRAGMA mmap_size = " + std::to_string(cfg.sqliteMmapSize)).c_str());

    CDatabaseSchema::Create(m_db.get());
    PrepareStatements();
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
    if (sqlite3_exec(m_db.get(), sql, nullptr, nullptr, &err) != SQLITE_OK)
    {
        std::string msg = err;
        sqlite3_free(err);
        LOG_ERROR("SQL error: {} | Query: {}", msg, sql);
        throw CDbError("SQL error: " + msg);
    }
}

void CDatabase::PrepareStatements()
{
    auto prep = [&](std::string_view sql, sqlite3_stmt_ptr& stmt)
    {
        sqlite3_stmt* raw = nullptr;
        Check(sqlite3_prepare_v2(m_db.get(), sql.data(), -1, &raw, nullptr), "prepare");
        stmt.reset(raw);
    };

    prep(Sql::InsertAuthor, m_stmtInsertAuthor);
    prep(Sql::GetAuthorId, m_stmtGetAuthor);
    prep(Sql::InsertGenre, m_stmtInsertGenre);
    prep(Sql::GetGenreId, m_stmtGetGenre);
    prep(Sql::InsertSeries, m_stmtInsertSeries);
    prep(Sql::GetSeriesId, m_stmtGetSeries);
    prep(Sql::InsertArchive, m_stmtInsertArchive);
    prep(Sql::GetArchiveId, m_stmtGetArchive);
    prep(Sql::InsertBook, m_stmtInsertBook);
    prep(Sql::InsertBookAuthor, m_stmtInsertBookAuthor);
    prep(Sql::InsertBookGenre, m_stmtInsertBookGenre);
    prep(Sql::CheckBookExists, m_stmtBookExists);
    prep(Sql::UpdateBookFb2, m_stmtUpdateFb2);
    prep(Sql::GetBookPath, m_stmtGetBookPath);
    
    // Publishers
    prep(Sql::InsertPublisher, m_stmtInsertPublisher);
    prep(Sql::GetPublisherId, m_stmtGetPublisher);
}

int64_t CDatabase::GetOrCreateAuthor(const Inpx::SAuthor& author)
{
    std::string key = author.lastName + "|" + author.firstName + "|" + author.middleName;
    auto it = m_cacheAuthors.find(key);
    if (it != m_cacheAuthors.end()) return it->second;

    sqlite3_reset(m_stmtInsertAuthor.get());
    sqlite3_bind_text(m_stmtInsertAuthor.get(), 1, author.lastName.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(m_stmtInsertAuthor.get(), 2, author.firstName.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(m_stmtInsertAuthor.get(), 3, author.middleName.c_str(), -1, SQLITE_TRANSIENT);
    Check(sqlite3_step(m_stmtInsertAuthor.get()), "InsertAuthor");

    sqlite3_reset(m_stmtGetAuthor.get());
    sqlite3_bind_text(m_stmtGetAuthor.get(), 1, author.lastName.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(m_stmtGetAuthor.get(), 2, author.firstName.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(m_stmtGetAuthor.get(), 3, author.middleName.c_str(), -1, SQLITE_TRANSIENT);
    
    int64_t id = 0;
    if (sqlite3_step(m_stmtGetAuthor.get()) == SQLITE_ROW)
        id = sqlite3_column_int64(m_stmtGetAuthor.get(), 0);
    
    if (id > 0) m_cacheAuthors[key] = id;
    return id;
}

int64_t CDatabase::GetOrCreateGenre(const std::string& genre)
{
    auto it = m_cacheGenres.find(genre);
    if (it != m_cacheGenres.end()) return it->second;

    sqlite3_reset(m_stmtInsertGenre.get());
    sqlite3_bind_text(m_stmtInsertGenre.get(), 1, genre.c_str(), -1, SQLITE_TRANSIENT);
    Check(sqlite3_step(m_stmtInsertGenre.get()), "InsertGenre");

    sqlite3_reset(m_stmtGetGenre.get());
    sqlite3_bind_text(m_stmtGetGenre.get(), 1, genre.c_str(), -1, SQLITE_TRANSIENT);
    
    int64_t id = 0;
    if (sqlite3_step(m_stmtGetGenre.get()) == SQLITE_ROW)
        id = sqlite3_column_int64(m_stmtGetGenre.get(), 0);
    
    if (id > 0) m_cacheGenres[genre] = id;
    return id;
}

int64_t CDatabase::GetOrCreateSeries(const std::string& series)
{
    if (series.empty()) return 0;
    auto it = m_cacheSeries.find(series);
    if (it != m_cacheSeries.end()) return it->second;

    sqlite3_reset(m_stmtInsertSeries.get());
    sqlite3_bind_text(m_stmtInsertSeries.get(), 1, series.c_str(), -1, SQLITE_TRANSIENT);
    Check(sqlite3_step(m_stmtInsertSeries.get()), "InsertSeries");

    sqlite3_reset(m_stmtGetSeries.get());
    sqlite3_bind_text(m_stmtGetSeries.get(), 1, series.c_str(), -1, SQLITE_TRANSIENT);
    
    int64_t id = 0;
    if (sqlite3_step(m_stmtGetSeries.get()) == SQLITE_ROW)
        id = sqlite3_column_int64(m_stmtGetSeries.get(), 0);
    
    if (id > 0) m_cacheSeries[series] = id;
    return id;
}

int64_t CDatabase::GetOrCreatePublisher(const std::string& pub)
{
    if (pub.empty()) return 0;
    auto it = m_cachePublishers.find(pub);
    if (it != m_cachePublishers.end()) return it->second;

    sqlite3_reset(m_stmtInsertPublisher.get());
    sqlite3_bind_text(m_stmtInsertPublisher.get(), 1, pub.c_str(), -1, SQLITE_TRANSIENT);
    Check(sqlite3_step(m_stmtInsertPublisher.get()), "InsertPublisher");

    sqlite3_reset(m_stmtGetPublisher.get());
    sqlite3_bind_text(m_stmtGetPublisher.get(), 1, pub.c_str(), -1, SQLITE_TRANSIENT);
    
    int64_t id = 0;
    if (sqlite3_step(m_stmtGetPublisher.get()) == SQLITE_ROW)
        id = sqlite3_column_int64(m_stmtGetPublisher.get(), 0);
    
    if (id > 0) m_cachePublishers[pub] = id;
    return id;
}

int64_t CDatabase::GetOrCreateArchive(const std::string& name)
{
    auto it = m_cacheArchives.find(name);
    if (it != m_cacheArchives.end()) return it->second;

    sqlite3_reset(m_stmtInsertArchive.get());
    sqlite3_bind_text(m_stmtInsertArchive.get(), 1, name.c_str(), -1, SQLITE_TRANSIENT);
    Check(sqlite3_step(m_stmtInsertArchive.get()), "InsertArchive");

    sqlite3_reset(m_stmtGetArchive.get());
    sqlite3_bind_text(m_stmtGetArchive.get(), 1, name.c_str(), -1, SQLITE_TRANSIENT);
    
    int64_t id = 0;
    if (sqlite3_step(m_stmtGetArchive.get()) == SQLITE_ROW)
        id = sqlite3_column_int64(m_stmtGetArchive.get(), 0);
    
    if (id > 0) m_cacheArchives[name] = id;
    return id;
}

int64_t CDatabase::InsertBook(const Inpx::SBookRecord& rec, const Fb2::SFb2Data& fb2)
{
    int64_t archId = GetOrCreateArchive(rec.archiveName);
    int64_t seriesId = GetOrCreateSeries(rec.series);
    int64_t pubId = GetOrCreatePublisher(fb2.publisher);

    sqlite3_reset(m_stmtInsertBook.get());
    sqlite3_bind_text(m_stmtInsertBook.get(), 1, rec.libId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(m_stmtInsertBook.get(), 2, archId);
    sqlite3_bind_text(m_stmtInsertBook.get(), 3, rec.title.c_str(), -1, SQLITE_TRANSIENT);
    
    if (seriesId > 0) sqlite3_bind_int64(m_stmtInsertBook.get(), 4, seriesId);
    else              sqlite3_bind_null(m_stmtInsertBook.get(), 4);

    sqlite3_bind_int(m_stmtInsertBook.get(), 5, rec.seriesNumber);
    sqlite3_bind_text(m_stmtInsertBook.get(), 6, rec.fileName.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(m_stmtInsertBook.get(), 7, rec.fileSize);
    sqlite3_bind_text(m_stmtInsertBook.get(), 8, rec.fileExt.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(m_stmtInsertBook.get(), 9, rec.dateAdded.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(m_stmtInsertBook.get(), 10, rec.language.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(m_stmtInsertBook.get(), 11, rec.rating);
    sqlite3_bind_text(m_stmtInsertBook.get(), 12, rec.keywords.c_str(), -1, SQLITE_TRANSIENT);

    sqlite3_bind_text(m_stmtInsertBook.get(), 13, fb2.annotation.c_str(), -1, SQLITE_TRANSIENT);
    
    if (pubId > 0) sqlite3_bind_int64(m_stmtInsertBook.get(), 14, pubId);
    else           sqlite3_bind_null(m_stmtInsertBook.get(), 14);

    sqlite3_bind_text(m_stmtInsertBook.get(), 15, fb2.isbn.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(m_stmtInsertBook.get(), 16, fb2.publishYear.c_str(), -1, SQLITE_TRANSIENT);
    
    if (sqlite3_step(m_stmtInsertBook.get()) != SQLITE_DONE)
        return 0;

    int64_t bookId = LastInsertRowId();

    for (const auto& author : rec.authors)
    {
        int64_t aid = GetOrCreateAuthor(author);
        sqlite3_reset(m_stmtInsertBookAuthor.get());
        sqlite3_bind_int64(m_stmtInsertBookAuthor.get(), 1, bookId);
        sqlite3_bind_int64(m_stmtInsertBookAuthor.get(), 2, aid);
        Check(sqlite3_step(m_stmtInsertBookAuthor.get()), "InsertBookAuthor");
    }

    for (const auto& genre : rec.genres)
    {
        int64_t gid = GetOrCreateGenre(genre);
        sqlite3_reset(m_stmtInsertBookGenre.get());
        sqlite3_bind_int64(m_stmtInsertBookGenre.get(), 1, bookId);
        sqlite3_bind_int64(m_stmtInsertBookGenre.get(), 2, gid);
        Check(sqlite3_step(m_stmtInsertBookGenre.get()), "InsertBookGenre");
    }

    return bookId;
}

bool CDatabase::BookExists(const std::string& libId, const std::string& archiveName)
{
    int64_t archId = GetOrCreateArchive(archiveName);
    sqlite3_reset(m_stmtBookExists.get());
    sqlite3_bind_text(m_stmtBookExists.get(), 1, libId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(m_stmtBookExists.get(), 2, archId);
    return sqlite3_step(m_stmtBookExists.get()) == SQLITE_ROW;
}

std::vector<std::string> CDatabase::GetIndexedArchives()
{
    std::vector<std::string> res;
    sqlite3_stmt* raw = nullptr;
    Check(sqlite3_prepare_v2(m_db.get(), Sql::GetIndexedArchives.data(), -1, &raw, nullptr), "GetIndexedArchives prepare");
    sqlite3_stmt_ptr stmt(raw);

    while (sqlite3_step(stmt.get()) == SQLITE_ROW)
    {
        const auto* text = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 0));
        if (text) res.emplace_back(text);
    }
    return res;
}

void CDatabase::MarkArchiveIndexed(const std::string& archiveName)
{
    (void)GetOrCreateArchive(archiveName);
    sqlite3_stmt* raw = nullptr;
    Check(sqlite3_prepare_v2(m_db.get(), Sql::UpdateArchiveIndexed.data(), -1, &raw, nullptr), "MarkArchiveIndexed prepare");
    sqlite3_stmt_ptr stmt(raw);

    sqlite3_bind_text(stmt.get(), 1, archiveName.c_str(), -1, SQLITE_TRANSIENT);
    Check(sqlite3_step(stmt.get()), "MarkArchiveIndexed step");
}

int64_t CDatabase::CountBooks() const
{
    sqlite3_stmt* raw = nullptr;
    Check(sqlite3_prepare_v2(m_db.get(), Sql::CountBooks.data(), -1, &raw, nullptr), "CountBooks prepare");
    sqlite3_stmt_ptr stmt(raw);

    if (sqlite3_step(stmt.get()) == SQLITE_ROW)
        return sqlite3_column_int64(stmt.get(), 0);
    return 0;
}

int64_t CDatabase::CountAuthors() const
{
    sqlite3_stmt* raw = nullptr;
    Check(sqlite3_prepare_v2(m_db.get(), Sql::CountAuthors.data(), -1, &raw, nullptr), "CountAuthors prepare");
    sqlite3_stmt_ptr stmt(raw);

    if (sqlite3_step(stmt.get()) == SQLITE_ROW)
        return sqlite3_column_int64(stmt.get(), 0);
    return 0;
}

int64_t CDatabase::LastInsertRowId() const
{
    return sqlite3_last_insert_rowid(m_db.get());
}

void CDatabase::Check(int rc, const char* context)
{
    if (rc != SQLITE_OK && rc != SQLITE_DONE && rc != SQLITE_ROW)
        throw CDbError(std::string(context) + ": " + std::to_string(rc));
}

std::optional<SBookPath> CDatabase::GetBookPath(int64_t bookId)
{
    sqlite3_reset(m_stmtGetBookPath.get());
    sqlite3_bind_int64(m_stmtGetBookPath.get(), 1, bookId);
    
    if (sqlite3_step(m_stmtGetBookPath.get()) == SQLITE_ROW)
    {
        SBookPath bp;
        bp.archiveName = reinterpret_cast<const char*>(sqlite3_column_text(m_stmtGetBookPath.get(), 0));
        bp.fileName = reinterpret_cast<const char*>(sqlite3_column_text(m_stmtGetBookPath.get(), 1));
        return bp;
    }
    return std::nullopt;
}

void CDatabase::DropIndexes()
{
    LOG_INFO("Dropping indexes for bulk import...");
    Exec("DROP INDEX IF EXISTS idx_books_title");
    Exec("DROP INDEX IF EXISTS idx_books_lang");
}

void CDatabase::CreateIndexes()
{
    LOG_INFO("Re-creating indexes after import...");
    Exec(Sql::CreateIndexBooksTitle.data());
    Exec(Sql::CreateIndexBooksLang.data());
}

} // namespace Librium::Db
