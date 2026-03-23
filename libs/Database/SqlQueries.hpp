#pragma once

#include <string_view>

namespace Librium::Db::Sql {

// --- Schema Creation ---

// Set WAL journaling mode for better performance
constexpr std::string_view PragmaWal = "PRAGMA journal_mode = WAL";

// Set synchronous mode to NORMAL for a balance between speed and reliability
constexpr std::string_view PragmaSyncNormal = "PRAGMA synchronous = NORMAL";

// Bulk import mode — maximum write speed
constexpr std::string_view PragmaJournalOff = "PRAGMA journal_mode = OFF";
constexpr std::string_view PragmaSyncOff = "PRAGMA synchronous = OFF";

// Initial page size for new databases
constexpr std::string_view PragmaPageSize = "PRAGMA page_size = 16384";

// Performance pragmas (dynamic value appended at call site)
constexpr std::string_view PragmaCacheSizePrefix = "PRAGMA cache_size = ";
constexpr std::string_view PragmaTempStore = "PRAGMA temp_store = MEMORY";
constexpr std::string_view PragmaMmapSizePrefix = "PRAGMA mmap_size = ";
constexpr std::string_view PragmaBusyTimeout = "PRAGMA busy_timeout = 5000";

// Transaction control
constexpr std::string_view BeginTransaction = "BEGIN TRANSACTION";
constexpr std::string_view CommitTransaction = "COMMIT";
constexpr std::string_view RollbackTransaction = "ROLLBACK";

// Create archives table
constexpr std::string_view CreateTableArchives = R"(
    CREATE TABLE IF NOT EXISTS archives (
        id INTEGER PRIMARY KEY,
        name TEXT UNIQUE,
        last_indexed TEXT
    )
)";

// Create authors table
constexpr std::string_view CreateTableAuthors = R"(
    CREATE TABLE IF NOT EXISTS authors (
        id INTEGER PRIMARY KEY,
        last_name TEXT,
        first_name TEXT,
        middle_name TEXT,
        search_name TEXT,
        UNIQUE(last_name, first_name, middle_name)
    )
)";

// Create genres table
constexpr std::string_view CreateTableGenres = R"(
    CREATE TABLE IF NOT EXISTS genres (
        id INTEGER PRIMARY KEY,
        code TEXT UNIQUE
    )
)";

// Create series table
constexpr std::string_view CreateTableSeries = R"(
    CREATE TABLE IF NOT EXISTS series (
        id INTEGER PRIMARY KEY,
        name TEXT UNIQUE,
        search_name TEXT
    )
)";

// Create publishers table
constexpr std::string_view CreateTablePublishers = R"(
    CREATE TABLE IF NOT EXISTS publishers (
        id INTEGER PRIMARY KEY,
        name TEXT UNIQUE
    )
)";

// Create main books table (without covers)
constexpr std::string_view CreateTableBooks = R"(
    CREATE TABLE IF NOT EXISTS books (
        id INTEGER PRIMARY KEY,
        lib_id TEXT,
        archive_id INTEGER,
        title TEXT,
        search_title TEXT,
        series_id INTEGER,
        series_no INTEGER,
        file_name TEXT,
        file_size INTEGER,
        file_ext TEXT,
        date_added TEXT,
        language TEXT,
        rating INTEGER,
        keywords TEXT,
        annotation TEXT,
        publisher_id INTEGER,
        isbn TEXT,
        publish_date TEXT,
        UNIQUE(lib_id, archive_id),
        FOREIGN KEY(archive_id) REFERENCES archives(id),
        FOREIGN KEY(series_id) REFERENCES series(id),
        FOREIGN KEY(publisher_id) REFERENCES publishers(id)
    )
)";

// Create book-author relation table
constexpr std::string_view CreateTableBookAuthors = R"(
    CREATE TABLE IF NOT EXISTS book_authors (
        book_id INTEGER,
        author_id INTEGER,
        PRIMARY KEY(book_id, author_id),
        FOREIGN KEY(book_id) REFERENCES books(id),
        FOREIGN KEY(author_id) REFERENCES authors(id)
    )
)";

// Create book-genre relation table
constexpr std::string_view CreateTableBookGenres = R"(
    CREATE TABLE IF NOT EXISTS book_genres (
        book_id INTEGER,
        genre_id INTEGER,
        PRIMARY KEY(book_id, genre_id),
        FOREIGN KEY(book_id) REFERENCES books(id),
        FOREIGN KEY(genre_id) REFERENCES genres(id)
    )
)";

// Index for faster title searches
constexpr std::string_view CreateIndexBooksTitle = "CREATE INDEX IF NOT EXISTS idx_books_title ON books(title)";

// Index for faster language filtering
constexpr std::string_view CreateIndexBooksLang = "CREATE INDEX IF NOT EXISTS idx_books_lang ON books(language)";

// Search columns indexes
constexpr std::string_view CreateIndexBooksSearchTitle = "CREATE INDEX IF NOT EXISTS idx_books_search_title ON books(search_title)";
constexpr std::string_view CreateIndexAuthorsSearchName = "CREATE INDEX IF NOT EXISTS idx_authors_search_name ON authors(search_name)";
constexpr std::string_view CreateIndexSeriesSearchName = "CREATE INDEX IF NOT EXISTS idx_series_search_name ON series(search_name)";

// Migration helpers: add search_name to existing series tables created before this column was introduced.
// Use CheckSeriesSearchNameColumn to guard against "duplicate column name" SQLite errors.
constexpr std::string_view CheckSeriesSearchNameColumn = "SELECT COUNT(*) FROM pragma_table_info('series') WHERE name='search_name'";
constexpr std::string_view MigrateSeriesAddSearchName = "ALTER TABLE series ADD COLUMN search_name TEXT";

// Drop indexes for bulk import
constexpr std::string_view DropIndexBooksTitle = "DROP INDEX IF EXISTS idx_books_title";
constexpr std::string_view DropIndexBooksLang = "DROP INDEX IF EXISTS idx_books_lang";
constexpr std::string_view DropIndexBooksSearchTitle = "DROP INDEX IF EXISTS idx_books_search_title";
constexpr std::string_view DropIndexAuthorsSearchName = "DROP INDEX IF EXISTS idx_authors_search_name";
constexpr std::string_view DropIndexSeriesSearchName = "DROP INDEX IF EXISTS idx_series_search_name";


// --- Data Operations ---

// Insert an author (if not exists)
constexpr std::string_view InsertAuthor = "INSERT OR IGNORE INTO authors (last_name, first_name, middle_name, search_name) VALUES (?, ?, ?, librium_upper(?))";

// Get author ID by full name
constexpr std::string_view GetAuthorId = "SELECT id FROM authors WHERE last_name=? AND first_name=? AND middle_name=?";

// Insert a genre
constexpr std::string_view InsertGenre = "INSERT OR IGNORE INTO genres (code) VALUES (?)";

// Get genre ID by code
constexpr std::string_view GetGenreId = "SELECT id FROM genres WHERE code=?";

// Insert a series (if not exists) — search_name is pre-uppercased for Cyrillic-safe search
constexpr std::string_view InsertSeries = "INSERT OR IGNORE INTO series (name, search_name) VALUES (?, librium_upper(?))";

// Get series ID by name
constexpr std::string_view GetSeriesId = "SELECT id FROM series WHERE name=?";

// Insert a publisher
constexpr std::string_view InsertPublisher = "INSERT OR IGNORE INTO publishers (name) VALUES (?)";

// Get publisher ID by name
constexpr std::string_view GetPublisherId = "SELECT id FROM publishers WHERE name=?";

// Insert an archive record
constexpr std::string_view InsertArchive = "INSERT OR IGNORE INTO archives (name) VALUES (?)";

// Get archive ID by name
constexpr std::string_view GetArchiveId = "SELECT id FROM archives WHERE name=?";

// Insert a new book (no cover)
constexpr std::string_view InsertBook = R"(
    INSERT INTO books (
        lib_id, archive_id, title, search_title, series_id, series_no,
        file_name, file_size, file_ext, date_added, language,
        rating, keywords, annotation, publisher_id, isbn,
        publish_date
    ) VALUES (?, ?, ?, librium_upper(?), ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
)";

// Link an author to a book
constexpr std::string_view InsertBookAuthor = "INSERT OR IGNORE INTO book_authors (book_id, author_id) VALUES (?, ?)";

// Link a genre to a book
constexpr std::string_view InsertBookGenre = "INSERT OR IGNORE INTO book_genres (book_id, genre_id) VALUES (?, ?)";

// Check if a book already exists
constexpr std::string_view CheckBookExists = "SELECT id FROM books WHERE lib_id=? AND archive_id=?";

// Update book metadata from FB2 (no cover)
constexpr std::string_view UpdateBookFb2 = R"(
    UPDATE books SET 
        annotation=?, publisher_id=?, isbn=?, 
        publish_date=?
    WHERE id=?
)";

// Get archive and file path for a book
constexpr std::string_view GetBookPath = R"(
    SELECT arch.name, b.file_name || '.' || b.file_ext
    FROM books b
    JOIN archives arch ON b.archive_id = arch.id
    WHERE b.id = ?
)";

// Get names of all indexed archives
constexpr std::string_view GetIndexedArchives = "SELECT name FROM archives WHERE last_indexed IS NOT NULL";

// Update last indexed date for an archive
constexpr std::string_view UpdateArchiveIndexed = "UPDATE archives SET last_indexed=datetime('now') WHERE name=?";

// Total count of books in DB
constexpr std::string_view CountBooks = "SELECT COUNT(*) FROM books";

// Total count of authors in DB
constexpr std::string_view CountAuthors = "SELECT COUNT(*) FROM authors";

// Count of book-related indexes (used by tests to verify index lifecycle)
constexpr std::string_view CountBookIndexes = "SELECT COUNT(*) FROM sqlite_master WHERE type='index' AND name LIKE 'idx_books_%'";


// --- Querying ---

// Fetch all authors for a specific book
constexpr std::string_view FetchBookAuthors = R"(
    SELECT a.last_name, a.first_name, a.middle_name
    FROM authors a
    JOIN book_authors ba ON a.id = ba.author_id
    WHERE ba.book_id = ?
)";

// Fetch all genres for a specific book
constexpr std::string_view FetchBookGenres = R"(
    SELECT g.code
    FROM genres g
    JOIN book_genres bg ON g.id = bg.genre_id
    WHERE bg.book_id = ?
)";

// Base SELECT for books
constexpr std::string_view QuerySelectBooksBase = 
    "SELECT b.id, b.lib_id, b.title, ser.name, b.series_no, b.file_name, b.file_size, b.file_ext, "
    "b.date_added, b.language, b.rating, b.keywords, b.annotation, arch.name, pub.name, b.isbn, b.publish_date "
    "FROM books b "
    "JOIN archives arch ON b.archive_id = arch.id "
    "LEFT JOIN series ser ON b.series_id = ser.id "
    "LEFT JOIN publishers pub ON b.publisher_id = pub.id ";

// Base COUNT for books
constexpr std::string_view QueryCountBooksBase = 
    "SELECT COUNT(*) FROM books b "
    "JOIN archives arch ON b.archive_id = arch.id "
    "LEFT JOIN series ser ON b.series_id = ser.id "
    "LEFT JOIN publishers pub ON b.publisher_id = pub.id ";

// Dynamic JOINs
constexpr std::string_view QueryJoinAuthors = "JOIN book_authors ba ON b.id = ba.book_id JOIN authors a ON ba.author_id = a.id ";
constexpr std::string_view QueryJoinGenres = "JOIN book_genres bg ON b.id = bg.book_id JOIN genres g ON bg.genre_id = g.id ";

// Dynamic WHERE clauses
constexpr std::string_view QueryWhere1 = "WHERE 1=1 ";
constexpr std::string_view QueryWhereGenre = "AND g.code = ? ";
constexpr std::string_view QueryWhereLanguage = "AND b.language = ? ";
constexpr std::string_view QueryWhereLibId = "AND b.lib_id = ? ";
constexpr std::string_view QueryWhereArchiveName = "AND arch.name = ? ";
constexpr std::string_view QueryWhereDateFrom = "AND b.date_added >= ? ";
constexpr std::string_view QueryWhereDateTo = "AND b.date_added <= ? ";
constexpr std::string_view QueryWhereRatingMin = "AND b.rating >= ? ";
constexpr std::string_view QueryWhereRatingMax = "AND b.rating <= ? ";
constexpr std::string_view QueryWhereWithAnnotation = "AND b.annotation IS NOT NULL AND b.annotation != '' ";
constexpr std::string_view QueryWhereId = "WHERE b.id = ? ";

// Order and Limit
constexpr std::string_view QueryOrderTitle = "ORDER BY b.title ";

} // namespace Librium::Db::Sql
