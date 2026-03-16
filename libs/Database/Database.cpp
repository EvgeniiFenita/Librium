#include "Database.hpp"

#include <sqlite3.h>

#include <stdexcept>

namespace {

const char* k_schema = R"SQL(
PRAGMA journal_mode = WAL;
PRAGMA synchronous  = NORMAL;
PRAGMA foreign_keys = ON;
PRAGMA cache_size   = -64000;
PRAGMA temp_store   = MEMORY;
PRAGMA mmap_size    = 268435456;

CREATE TABLE IF NOT EXISTS authors (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    last_name   TEXT NOT NULL DEFAULT '',
    first_name  TEXT NOT NULL DEFAULT '',
    middle_name TEXT NOT NULL DEFAULT '',
    UNIQUE(last_name, first_name, middle_name)
);
CREATE TABLE IF NOT EXISTS genres (
    id   INTEGER PRIMARY KEY AUTOINCREMENT,
    code TEXT NOT NULL UNIQUE
);
CREATE TABLE IF NOT EXISTS series (
    id   INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL UNIQUE
);
CREATE TABLE IF NOT EXISTS publishers (
    id   INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL UNIQUE
);
CREATE TABLE IF NOT EXISTS archives (
    id           INTEGER PRIMARY KEY AUTOINCREMENT,
    name         TEXT NOT NULL UNIQUE,
    last_indexed TEXT NOT NULL DEFAULT ''
);
CREATE TABLE IF NOT EXISTS books (
    id            INTEGER PRIMARY KEY AUTOINCREMENT,
    lib_id        TEXT    NOT NULL,
    archive_id    INTEGER REFERENCES archives(id),
    title         TEXT    NOT NULL DEFAULT '',
    series_id     INTEGER REFERENCES series(id),
    series_number INTEGER DEFAULT 0,
    file_name     TEXT    NOT NULL DEFAULT '',
    file_ext      TEXT    NOT NULL DEFAULT 'fb2',
    file_size     INTEGER DEFAULT 0,
    language      TEXT    DEFAULT '',
    date_added    TEXT    DEFAULT '',
    rating        INTEGER DEFAULT 0,
    keywords      TEXT    DEFAULT '',
    annotation    TEXT    DEFAULT '',
    publisher_id  INTEGER REFERENCES publishers(id),
    isbn          TEXT    DEFAULT '',
    publish_year  TEXT    DEFAULT '',
    cover         BLOB,
    cover_mime    TEXT    DEFAULT '',
    UNIQUE(lib_id, archive_id)
);
CREATE TABLE IF NOT EXISTS book_authors (
    book_id   INTEGER NOT NULL REFERENCES books(id)   ON DELETE CASCADE,
    author_id INTEGER NOT NULL REFERENCES authors(id),
    PRIMARY KEY(book_id, author_id)
);
CREATE TABLE IF NOT EXISTS book_genres (
    book_id  INTEGER NOT NULL REFERENCES books(id)  ON DELETE CASCADE,
    genre_id INTEGER NOT NULL REFERENCES genres(id),
    PRIMARY KEY(book_id, genre_id)
);
CREATE INDEX IF NOT EXISTS idx_books_title    ON books(title);
CREATE INDEX IF NOT EXISTS idx_books_language ON books(language);
CREATE INDEX IF NOT EXISTS idx_books_lib_id   ON books(lib_id);
CREATE INDEX IF NOT EXISTS idx_books_series   ON books(series_id);
CREATE INDEX IF NOT EXISTS idx_authors_name   ON authors(last_name, first_name);
CREATE INDEX IF NOT EXISTS idx_book_authors   ON book_authors(author_id);
CREATE INDEX IF NOT EXISTS idx_book_genres    ON book_genres(genre_id);
)SQL";

} // namespace

namespace Librium::Db {

void CDatabase::Check(int rc, const char* context) 
{
    if (rc != SQLITE_OK && rc != SQLITE_ROW && rc != SQLITE_DONE)
        throw CDbError(std::string(context) + ": rc=" + std::to_string(rc));
}

CDatabase::CDatabase(const std::string& path) 
{
    int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX;
    Check(sqlite3_open_v2(path.c_str(), &m_db, flags, nullptr), "open");
    CreateSchema();
    PrepareStatements();
}

CDatabase::~CDatabase() 
{
    FinalizeStatements();
    if (m_db) sqlite3_close_v2(m_db);
}

void CDatabase::Exec(const char* sql) 
{
    char* err = nullptr;
    int rc = sqlite3_exec(m_db, sql, nullptr, nullptr, &err);
    if (rc != SQLITE_OK) 
{
        std::string msg = err ? err : "unknown";
        sqlite3_free(err);
        throw CDbError(std::string("exec failed: ") + msg);
    }
}

void CDatabase::CreateSchema() 
{ Exec(k_schema); }

void CDatabase::BeginTransaction() 
{ Exec("BEGIN"); }
void CDatabase::Commit() 
{ Exec("COMMIT"); }
void CDatabase::Rollback() 
{ Exec("ROLLBACK"); }

int64_t CDatabase::LastInsertRowId() const
{ return sqlite3_last_insert_rowid(m_db); }

void CDatabase::PrepareStatements() 
{
    auto P = [&](sqlite3_stmt*& s, const char* sql) 
{ Check(sqlite3_prepare_v2(m_db, sql, -1, &s, nullptr), sql); };

    P(m_stmtBookExists,
        "SELECT 1 FROM books b JOIN archives a ON a.id=b.archive_id "
        "WHERE b.lib_id=?1 AND a.name=?2 LIMIT 1");
    P(m_stmtInsertAuthor,
        "INSERT OR IGNORE INTO authors(last_name,first_name,middle_name) VALUES(?1,?2,?3)");
    P(m_stmtGetAuthor,
        "SELECT id FROM authors WHERE last_name=?1 AND first_name=?2 AND middle_name=?3");
    P(m_stmtInsertGenre, "INSERT OR IGNORE INTO genres(code) VALUES(?1)");
    P(m_stmtGetGenre,    "SELECT id FROM genres WHERE code=?1");
    P(m_stmtInsertArchive,
        "INSERT OR IGNORE INTO archives(name,last_indexed) VALUES(?1,'')");
    P(m_stmtGetArchive,  "SELECT id FROM archives WHERE name=?1");
    P(m_stmtInsertBook,
        "INSERT OR IGNORE INTO books("
        "lib_id,archive_id,title,series_id,series_number,"
        "file_name,file_ext,file_size,language,date_added,"
        "rating,keywords,annotation,publisher_id,isbn,publish_year,cover,cover_mime"
        ") VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11,?12,?13,?14,?15,?16,?17,?18)");
    P(m_stmtInsertBookAuthor,
        "INSERT OR IGNORE INTO book_authors(book_id,author_id) VALUES(?1,?2)");
    P(m_stmtInsertBookGenre,
        "INSERT OR IGNORE INTO book_genres(book_id,genre_id) VALUES(?1,?2)");
    P(m_stmtUpdateFb2,
        "UPDATE books SET annotation=?1,publisher_id=?2,isbn=?3,publish_year=?4,"
        "cover=?5,cover_mime=?6 WHERE id=?7");
}

void CDatabase::FinalizeStatements() 
{
    auto F = [](sqlite3_stmt*& s) 
{ if(s) 
{ sqlite3_finalize(s); s=nullptr; } };
    F(m_stmtBookExists); F(m_stmtInsertAuthor); F(m_stmtGetAuthor);
    F(m_stmtInsertGenre); F(m_stmtGetGenre); F(m_stmtInsertArchive);
    F(m_stmtGetArchive); F(m_stmtInsertBook); F(m_stmtInsertBookAuthor);
    F(m_stmtInsertBookGenre); F(m_stmtUpdateFb2);
}

int64_t CDatabase::GetOrCreateAuthor(const Inpx::CAuthor& a) 
{
    sqlite3_reset(m_stmtInsertAuthor);
    sqlite3_bind_text(m_stmtInsertAuthor,1,a.lastName.c_str(),-1,SQLITE_STATIC);
    sqlite3_bind_text(m_stmtInsertAuthor,2,a.firstName.c_str(),-1,SQLITE_STATIC);
    sqlite3_bind_text(m_stmtInsertAuthor,3,a.middleName.c_str(),-1,SQLITE_STATIC);
    sqlite3_step(m_stmtInsertAuthor);

    sqlite3_reset(m_stmtGetAuthor);
    sqlite3_bind_text(m_stmtGetAuthor,1,a.lastName.c_str(),-1,SQLITE_STATIC);
    sqlite3_bind_text(m_stmtGetAuthor,2,a.firstName.c_str(),-1,SQLITE_STATIC);
    sqlite3_bind_text(m_stmtGetAuthor,3,a.middleName.c_str(),-1,SQLITE_STATIC);
    sqlite3_step(m_stmtGetAuthor);
    return sqlite3_column_int64(m_stmtGetAuthor, 0);
}

int64_t CDatabase::GetOrCreateGenre(const std::string& genre) 
{
    sqlite3_reset(m_stmtInsertGenre);
    sqlite3_bind_text(m_stmtInsertGenre,1,genre.c_str(),-1,SQLITE_STATIC);
    sqlite3_step(m_stmtInsertGenre);

    sqlite3_reset(m_stmtGetGenre);
    sqlite3_bind_text(m_stmtGetGenre,1,genre.c_str(),-1,SQLITE_STATIC);
    sqlite3_step(m_stmtGetGenre);
    return sqlite3_column_int64(m_stmtGetGenre, 0);
}

int64_t CDatabase::GetOrCreateSeries(const std::string& series) 
{
    if (series.empty()) return 0;
    sqlite3_stmt* s = nullptr;
    sqlite3_prepare_v2(m_db,"INSERT OR IGNORE INTO series(name) VALUES(?)",-1,&s,nullptr);
    sqlite3_bind_text(s,1,series.c_str(),-1,SQLITE_STATIC);
    sqlite3_step(s); sqlite3_finalize(s);

    sqlite3_prepare_v2(m_db,"SELECT id FROM series WHERE name=?",-1,&s,nullptr);
    sqlite3_bind_text(s,1,series.c_str(),-1,SQLITE_STATIC);
    sqlite3_step(s);
    int64_t id = sqlite3_column_int64(s,0);
    sqlite3_finalize(s);
    return id;
}

int64_t CDatabase::GetOrCreatePublisher(const std::string& pub) 
{
    if (pub.empty()) return 0;
    sqlite3_stmt* s = nullptr;
    sqlite3_prepare_v2(m_db,"INSERT OR IGNORE INTO publishers(name) VALUES(?)",-1,&s,nullptr);
    sqlite3_bind_text(s,1,pub.c_str(),-1,SQLITE_STATIC);
    sqlite3_step(s); sqlite3_finalize(s);

    sqlite3_prepare_v2(m_db,"SELECT id FROM publishers WHERE name=?",-1,&s,nullptr);
    sqlite3_bind_text(s,1,pub.c_str(),-1,SQLITE_STATIC);
    sqlite3_step(s);
    int64_t id = sqlite3_column_int64(s,0);
    sqlite3_finalize(s);
    return id;
}

int64_t CDatabase::GetOrCreateArchive(const std::string& archiveName) 
{
    sqlite3_reset(m_stmtInsertArchive);
    sqlite3_bind_text(m_stmtInsertArchive,1,archiveName.c_str(),-1,SQLITE_STATIC);
    sqlite3_step(m_stmtInsertArchive);

    sqlite3_reset(m_stmtGetArchive);
    sqlite3_bind_text(m_stmtGetArchive,1,archiveName.c_str(),-1,SQLITE_STATIC);
    sqlite3_step(m_stmtGetArchive);
    return sqlite3_column_int64(m_stmtGetArchive, 0);
}

bool CDatabase::BookExists(const std::string& libId, const std::string& archiveName) 
{
    sqlite3_reset(m_stmtBookExists);
    sqlite3_bind_text(m_stmtBookExists,1,libId.c_str(),-1,SQLITE_STATIC);
    sqlite3_bind_text(m_stmtBookExists,2,archiveName.c_str(),-1,SQLITE_STATIC);
    return sqlite3_step(m_stmtBookExists) == SQLITE_ROW;
}

int64_t CDatabase::InsertBook(const Inpx::CBookRecord& rec, const Fb2::CFb2Data& fb2) 
{
    int64_t archiveId   = GetOrCreateArchive(rec.archiveName);
    int64_t seriesId    = GetOrCreateSeries(rec.series);
    int64_t publisherId = GetOrCreatePublisher(fb2.publisher);

    sqlite3_reset(m_stmtInsertBook);
    sqlite3_bind_text(m_stmtInsertBook,  1, rec.libId.c_str(),     -1, SQLITE_STATIC);
    sqlite3_bind_int64(m_stmtInsertBook, 2, archiveId);
    sqlite3_bind_text(m_stmtInsertBook,  3, rec.title.c_str(),     -1, SQLITE_STATIC);
    seriesId ? sqlite3_bind_int64(m_stmtInsertBook,4,seriesId)
             : sqlite3_bind_null(m_stmtInsertBook,4);
    sqlite3_bind_int(m_stmtInsertBook,   5, rec.seriesNumber);
    sqlite3_bind_text(m_stmtInsertBook,  6, rec.fileName.c_str(),  -1, SQLITE_STATIC);
    sqlite3_bind_text(m_stmtInsertBook,  7, rec.fileExt.c_str(),   -1, SQLITE_STATIC);
    sqlite3_bind_int64(m_stmtInsertBook, 8, static_cast<int64_t>(rec.fileSize));
    sqlite3_bind_text(m_stmtInsertBook,  9, rec.language.c_str(),  -1, SQLITE_STATIC);
    sqlite3_bind_text(m_stmtInsertBook, 10, rec.dateAdded.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(m_stmtInsertBook,  11, rec.rating);
    sqlite3_bind_text(m_stmtInsertBook, 12, rec.keywords.c_str(),  -1, SQLITE_STATIC);
    sqlite3_bind_text(m_stmtInsertBook, 13, fb2.annotation.c_str(),-1, SQLITE_STATIC);
    publisherId ? sqlite3_bind_int64(m_stmtInsertBook,14,publisherId)
                : sqlite3_bind_null(m_stmtInsertBook,14);
    sqlite3_bind_text(m_stmtInsertBook, 15, fb2.isbn.c_str(),        -1, SQLITE_STATIC);
    sqlite3_bind_text(m_stmtInsertBook, 16, fb2.publishYear.c_str(), -1, SQLITE_STATIC);

    if (!fb2.coverData.empty()) 
{
        sqlite3_bind_blob(m_stmtInsertBook,17,
            fb2.coverData.data(), static_cast<int>(fb2.coverData.size()), SQLITE_STATIC);
        sqlite3_bind_text(m_stmtInsertBook,18, fb2.coverMime.c_str(),-1,SQLITE_STATIC);
    }
    else
    {
        sqlite3_bind_null(m_stmtInsertBook,17);
        sqlite3_bind_text(m_stmtInsertBook,18,"",-1,SQLITE_STATIC);
    }

    int rc = sqlite3_step(m_stmtInsertBook);
    if (rc != SQLITE_DONE && rc != SQLITE_CONSTRAINT)
        throw CDbError("InsertBook failed: rc=" + std::to_string(rc));

    if (sqlite3_changes(m_db) == 0)
        return 0;

    int64_t bookId = LastInsertRowId();
    if (bookId == 0) return 0;

    for (const auto& CAuthor : rec.authors) 
{
        int64_t aid = GetOrCreateAuthor(CAuthor);
        sqlite3_reset(m_stmtInsertBookAuthor);
        sqlite3_bind_int64(m_stmtInsertBookAuthor,1,bookId);
        sqlite3_bind_int64(m_stmtInsertBookAuthor,2,aid);
        sqlite3_step(m_stmtInsertBookAuthor);
    }

    for (const auto& genre : rec.genres) 
{
        int64_t gid = GetOrCreateGenre(genre);
        sqlite3_reset(m_stmtInsertBookGenre);
        sqlite3_bind_int64(m_stmtInsertBookGenre,1,bookId);
        sqlite3_bind_int64(m_stmtInsertBookGenre,2,gid);
        sqlite3_step(m_stmtInsertBookGenre);
    }

    return bookId;
}

void CDatabase::UpdateBookFb2(int64_t bookId, const Fb2::CFb2Data& fb2) 
{
    int64_t pubId = GetOrCreatePublisher(fb2.publisher);
    sqlite3_reset(m_stmtUpdateFb2);
    sqlite3_bind_text(m_stmtUpdateFb2,1,fb2.annotation.c_str(),-1,SQLITE_STATIC);
    pubId ? sqlite3_bind_int64(m_stmtUpdateFb2,2,pubId) : sqlite3_bind_null(m_stmtUpdateFb2,2);
    sqlite3_bind_text(m_stmtUpdateFb2,3,fb2.isbn.c_str(),-1,SQLITE_STATIC);
    sqlite3_bind_text(m_stmtUpdateFb2,4,fb2.publishYear.c_str(),-1,SQLITE_STATIC);
    if (!fb2.coverData.empty()) 
{
        sqlite3_bind_blob(m_stmtUpdateFb2,5,fb2.coverData.data(),
            static_cast<int>(fb2.coverData.size()),SQLITE_STATIC);
        sqlite3_bind_text(m_stmtUpdateFb2,6,fb2.coverMime.c_str(),-1,SQLITE_STATIC);
    }
    else { sqlite3_bind_null(m_stmtUpdateFb2,5); sqlite3_bind_text(m_stmtUpdateFb2,6,"",-1,SQLITE_STATIC); }
    sqlite3_bind_int64(m_stmtUpdateFb2,7,bookId);
    sqlite3_step(m_stmtUpdateFb2);
}

std::vector<std::string> CDatabase::GetIndexedArchives() 
{
    std::vector<std::string> result;
    sqlite3_stmt* s = nullptr;
    sqlite3_prepare_v2(m_db,
        "SELECT name FROM archives WHERE last_indexed != ''",-1,&s,nullptr);
    while (sqlite3_step(s) == SQLITE_ROW)
        result.emplace_back(reinterpret_cast<const char*>(sqlite3_column_text(s,0)));
    sqlite3_finalize(s);
    return result;
}

void CDatabase::MarkArchiveIndexed(const std::string& archiveName) 
{
    sqlite3_stmt* s = nullptr;
    sqlite3_prepare_v2(m_db,
        "UPDATE archives SET last_indexed=datetime('now') WHERE name=?",-1,&s,nullptr);
    sqlite3_bind_text(s,1,archiveName.c_str(),-1,SQLITE_STATIC);
    sqlite3_step(s); sqlite3_finalize(s);
}

bool CDatabase::ArchiveExists(const std::string& archiveName) 
{
    sqlite3_stmt* s = nullptr;
    sqlite3_prepare_v2(m_db,
        "SELECT 1 FROM archives WHERE name=? AND last_indexed!='' LIMIT 1",-1,&s,nullptr);
    sqlite3_bind_text(s,1,archiveName.c_str(),-1,SQLITE_STATIC);
    bool exists = (sqlite3_step(s) == SQLITE_ROW);
    sqlite3_finalize(s);
    return exists;
}

int64_t CDatabase::CountBooks() const
{
    sqlite3_stmt* s = nullptr;
    sqlite3_prepare_v2(m_db,"SELECT COUNT(*) FROM books",-1,&s,nullptr);
    sqlite3_step(s);
    int64_t n = sqlite3_column_int64(s,0);
    sqlite3_finalize(s);
    return n;
}

int64_t CDatabase::CountAuthors() const
{
    sqlite3_stmt* s = nullptr;
    sqlite3_prepare_v2(m_db,"SELECT COUNT(*) FROM authors",-1,&s,nullptr);
    sqlite3_step(s);
    int64_t n = sqlite3_column_int64(s,0);
    sqlite3_finalize(s);
    return n;
}

} // namespace Librium::Db






