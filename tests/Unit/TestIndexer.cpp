#include <catch2/catch_test_macros.hpp>
#include "Indexer/Indexer.hpp"
#include "Database/Database.hpp"
#include "TestUtils.hpp"

#include <filesystem>
#include <string>

using namespace Librium::Indexer;
using namespace Librium::Db;
using namespace Librium::Tests;
using namespace Librium::Config;

namespace {

// Builds one INP line for an INPX archive entry.
// Field order: authors, genres, title, series, series_no, file_name,
//              file_size, lib_id, deleted, ext, date, lang, rating,
//              keywords, reserved
std::string MakeInpLine(const std::string& libId,
                        const std::string& title,
                        const std::string& fileName)
{
    constexpr char SEP = '\x04';
    std::string line;
    line += "Author,Test,:";
    line += SEP; line += "sf:";
    line += SEP; line += title;
    line += SEP;                  // series
    line += SEP;                  // series_no
    line += SEP; line += fileName;
    line += SEP; line += "100000";
    line += SEP; line += libId;
    line += SEP; line += "0";     // not deleted
    line += SEP; line += "fb2";
    line += SEP; line += "2024-01-01";
    line += SEP; line += "ru";
    line += SEP; line += "4";
    line += SEP;                  // keywords
    line += SEP;                  // reserved
    line += "\r\n";
    return line;
}

} // namespace

TEST_CASE("Indexer Run() in Full mode imports books from a real archive", "[indexer]")
{
    CTempDir tempDir;

    std::string fb2Content = "<?xml version=\"1.0\"?><FictionBook><description></description><body><section><p>Test</p></section></body></FictionBook>";

    CreateTestZip(tempDir.GetPath() / "fb2-testarch.zip", {{"testbook1.fb2", fb2Content}});

    constexpr char SEP = '\x04';
    std::string inpLine;
    inpLine += "TestAuthor,First,:";
    inpLine += SEP; inpLine += "sf:";
    inpLine += SEP; inpLine += "Test Book Title";
    inpLine += SEP;
    inpLine += SEP;
    inpLine += SEP; inpLine += "testbook1";
    inpLine += SEP; inpLine += "100000";
    inpLine += SEP; inpLine += "111111";
    inpLine += SEP; inpLine += "0";
    inpLine += SEP; inpLine += "fb2";
    inpLine += SEP; inpLine += "2024-01-01";
    inpLine += SEP; inpLine += "ru";
    inpLine += SEP; inpLine += "4";
    inpLine += SEP;
    inpLine += SEP;
    inpLine += "\r\n";

    CreateTestZip(tempDir.GetPath() / "test.inpx", {
        {"fb2-testarch.zip.inp", inpLine},
        {"collection.info", "Test Library\ntest\n65536\n"},
        {"version.info", "20240101\r\n"}
    });

    SAppConfig cfg;
    cfg.database.path              = (tempDir.GetPath() / "test_run.db").string();
    cfg.library.inpxPath           = (tempDir.GetPath() / "test.inpx").string();
    cfg.library.archivesDir        = tempDir.GetPath().string();
    cfg.import.parseFb2            = false;
    cfg.import.threadCount         = 1;
    cfg.import.transactionBatchSize = 100;

    CDatabase db(cfg.database.path);
    CIndexer indexer(cfg);
    auto stats = indexer.Run(db, EImportMode::Full, nullptr);

    REQUIRE(stats.booksInserted == 1);
    REQUIRE(db.CountBooks() == 1);
    REQUIRE(db.GetIndexedArchives().size() == 1);
}

TEST_CASE("Indexer RequestStop() prevents book import", "[indexer]")
{
    CTempDir tempDir;

    std::string fb2Content = "<?xml version=\"1.0\"?><FictionBook><description></description><body><section><p>Test</p></section></body></FictionBook>";

    CreateTestZip(tempDir.GetPath() / "fb2-stoparch.zip", {{"testbook2.fb2", fb2Content}});

    constexpr char SEP = '\x04';
    std::string inpLine;
    inpLine += "TestAuthor,First,:";
    inpLine += SEP; inpLine += "sf:";
    inpLine += SEP; inpLine += "Test Book For Stop";
    inpLine += SEP;
    inpLine += SEP;
    inpLine += SEP; inpLine += "testbook2";
    inpLine += SEP; inpLine += "100000";
    inpLine += SEP; inpLine += "222222";
    inpLine += SEP; inpLine += "0";
    inpLine += SEP; inpLine += "fb2";
    inpLine += SEP; inpLine += "2024-01-01";
    inpLine += SEP; inpLine += "ru";
    inpLine += SEP; inpLine += "4";
    inpLine += SEP;
    inpLine += SEP;
    inpLine += "\r\n";

    CreateTestZip(tempDir.GetPath() / "stop.inpx", {
        {"fb2-stoparch.zip.inp", inpLine},
        {"collection.info", "Stop Library\nstop\n65536\n"},
        {"version.info", "20240101\r\n"}
    });

    SAppConfig cfg;
    cfg.database.path              = (tempDir.GetPath() / "test_stop.db").string();
    cfg.library.inpxPath           = (tempDir.GetPath() / "stop.inpx").string();
    cfg.library.archivesDir        = tempDir.GetPath().string();
    cfg.import.parseFb2            = false;
    cfg.import.threadCount         = 1;
    cfg.import.transactionBatchSize = 100;

    CDatabase db(cfg.database.path);
    CIndexer indexer(cfg);
    indexer.RequestStop();
    (void)indexer.Run(db, EImportMode::Full, nullptr);

    REQUIRE(db.CountBooks() == 0);
}

TEST_CASE("Indexer handles 0 new books gracefully (Index Preservation)", "[indexer]")
{
    CTempDir tempDir;
    std::string dbPath = (std::filesystem::path(tempDir.GetPath()) / "test_idx_bug.db").string();
    std::string inpxPath = (std::filesystem::path(tempDir.GetPath()) / "test_empty.inpx").string();

    CreateTestZip(inpxPath, {
        {"archive1.inp", "some data"}
    });

    SAppConfig cfg;
    cfg.database.path = dbPath;
    cfg.library.inpxPath = inpxPath;

    auto countIndexes = [](Librium::Db::CDatabase& database) -> int
    {
        return database.CountIndexes();
    };

    // 1. Initialize DB and verify indexes exist
    CDatabase db(dbPath);
    REQUIRE(countIndexes(db) > 0);
    
    // Mark archive as indexed so upgrade mode has 0 work
    db.MarkArchiveIndexed("archive1");

    // 2. Run indexer in upgrade mode (should find 0 work)
    CIndexer indexer(cfg);
    (void)indexer.Run(db, EImportMode::Upgrade, nullptr);

    // 3. Verify indexes STILL exist (they shouldn't have been dropped)
    REQUIRE(countIndexes(db) > 0);
}

// ---------------------------------------------------------------------------
// Upgrade mode archive-skip tests
// ---------------------------------------------------------------------------

TEST_CASE("Indexer Full import marks all processed archives as indexed", "[indexer]")
{
    CTempDir tempDir;

    const std::string inpA = MakeInpLine("A001", "Book A1", "bookA1")
                           + MakeInpLine("A002", "Book A2", "bookA2");
    const std::string inpB = MakeInpLine("B001", "Book B1", "bookB1");

    CreateTestZip(tempDir.GetPath() / "test.inpx", {
        {"archA.zip.inp", inpA},
        {"archB.zip.inp", inpB},
        {"collection.info", "Test\ntest\n65536\n"},
        {"version.info",    "20240101\r\n"}
    });

    SAppConfig cfg;
    cfg.database.path               = (tempDir.GetPath() / "mark.db").string();
    cfg.library.inpxPath            = (tempDir.GetPath() / "test.inpx").string();
    cfg.library.archivesDir         = tempDir.GetPath().string();
    cfg.import.parseFb2             = false;
    cfg.import.threadCount          = 1;
    cfg.import.transactionBatchSize = 100;

    CDatabase db(cfg.database.path);
    CIndexer  indexer(cfg);
    const auto stats = indexer.Run(db, EImportMode::Full, nullptr);

    REQUIRE(stats.booksInserted == 3);
    REQUIRE(db.CountBooks() == 3);

    const auto indexed = db.GetIndexedArchives();
    REQUIRE(indexed.size() == 2);
    const bool hasA = std::find(indexed.begin(), indexed.end(), "archA.zip") != indexed.end();
    const bool hasB = std::find(indexed.begin(), indexed.end(), "archB.zip") != indexed.end();
    REQUIRE(hasA);
    REQUIRE(hasB);
}

TEST_CASE("Indexer Upgrade skips already indexed archives during Run()", "[indexer]")
{
    CTempDir tempDir;

    const std::string inpA = MakeInpLine("A001", "Book A1", "bookA1")
                           + MakeInpLine("A002", "Book A2", "bookA2");
    const std::string inpB = MakeInpLine("B001", "Book B1", "bookB1");

    CreateTestZip(tempDir.GetPath() / "test.inpx", {
        {"archA.zip.inp", inpA},
        {"archB.zip.inp", inpB},
        {"collection.info", "Test\ntest\n65536\n"},
        {"version.info",    "20240101\r\n"}
    });

    SAppConfig cfg;
    cfg.database.path               = (tempDir.GetPath() / "upgrade_all.db").string();
    cfg.library.inpxPath            = (tempDir.GetPath() / "test.inpx").string();
    cfg.library.archivesDir         = tempDir.GetPath().string();
    cfg.import.parseFb2             = false;
    cfg.import.threadCount          = 1;
    cfg.import.transactionBatchSize = 100;

    CDatabase db(cfg.database.path);

    // First: full import
    {
        CIndexer full(cfg);
        const auto stats = full.Run(db, EImportMode::Full, nullptr);
        REQUIRE(stats.booksInserted == 3);
        REQUIRE(db.GetIndexedArchives().size() == 2);
    }

    // Second: upgrade — all archives are already indexed, nothing to do
    {
        CIndexer upgrade(cfg);
        const auto stats = upgrade.Run(db, EImportMode::Upgrade, nullptr);
        REQUIRE(stats.booksSkipped == 3);
        REQUIRE(stats.booksInserted == 0);
        REQUIRE(stats.archivesProcessed == 2);
        REQUIRE(db.CountBooks() == 3);
        REQUIRE(db.GetIndexedArchives().size() == 2);
    }
}

TEST_CASE("Indexer Upgrade only processes new archives added since last Full import", "[indexer]")
{
    CTempDir tempDir;

    const std::string inpA = MakeInpLine("A001", "Book A1", "bookA1")
                           + MakeInpLine("A002", "Book A2", "bookA2");
    const std::string inpB = MakeInpLine("B001", "Book B1", "bookB1");

    // Initial INPX: archive A only
    CreateTestZip(tempDir.GetPath() / "inpx_a_only.inpx", {
        {"archA.zip.inp", inpA},
        {"collection.info", "Test\ntest\n65536\n"},
        {"version.info",    "20240101\r\n"}
    });

    // Updated INPX: archives A + B
    CreateTestZip(tempDir.GetPath() / "inpx_ab.inpx", {
        {"archA.zip.inp", inpA},
        {"archB.zip.inp", inpB},
        {"collection.info", "Test\ntest\n65536\n"},
        {"version.info",    "20240101\r\n"}
    });

    SAppConfig cfg;
    cfg.database.path               = (tempDir.GetPath() / "upgrade_new.db").string();
    cfg.library.archivesDir         = tempDir.GetPath().string();
    cfg.import.parseFb2             = false;
    cfg.import.threadCount          = 1;
    cfg.import.transactionBatchSize = 100;

    CDatabase db(cfg.database.path);

    // Step 1: full import with archive A only
    cfg.library.inpxPath = (tempDir.GetPath() / "inpx_a_only.inpx").string();
    {
        CIndexer full(cfg);
        const auto stats = full.Run(db, EImportMode::Full, nullptr);
        REQUIRE(stats.booksInserted == 2);
        REQUIRE(db.GetIndexedArchives().size() == 1);
    }
    REQUIRE(db.CountBooks() == 2);

    // Step 2: upgrade with INPX now containing A + B
    // Archive A is already indexed → must be skipped entirely.
    // Archive B is new → its 1 book must be inserted.
    cfg.library.inpxPath = (tempDir.GetPath() / "inpx_ab.inpx").string();
    {
        CIndexer upgrade(cfg);
        const auto stats = upgrade.Run(db, EImportMode::Upgrade, nullptr);
        REQUIRE(stats.booksInserted == 1);
    }
    REQUIRE(db.CountBooks() == 3);
    REQUIRE(db.GetIndexedArchives().size() == 2);
}
