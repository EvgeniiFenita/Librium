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

TEST_CASE("Indexer basic operations", "[indexer]")
{
    CTempDir tempDir;
    std::string dbPath = (std::filesystem::path(tempDir.GetPath()) / "test_indexer.db").string();
    std::string inpxPath = (std::filesystem::path(tempDir.GetPath()) / "test.inpx").string();

    // Create a mock INPX file with two archives
    // Note: GetNewArchives uses stem() which removes the LAST extension.
    // In real INPX files are named like 'archive.inp', so stem() returns 'archive'.
    CreateTestZip(inpxPath, {
        {"archive1.inp", "some data"},
        {"archive2.inp", "some data"}
    });

    SAppConfig cfg;
    cfg.database.path = dbPath;
    cfg.library.inpxPath = inpxPath;

    SECTION("GetNewArchives identifies new archives correctly")
    {
        CDatabase db(dbPath);
        CIndexer indexer(cfg);

        // Initially, both should be new
        auto newArchives = indexer.GetNewArchives(db, inpxPath);
        REQUIRE(newArchives.size() == 2);
        
        bool hasArchive1 = false;
        bool hasArchive2 = false;
        for (const auto& a : newArchives) {
            if (a == "archive1") hasArchive1 = true;
            if (a == "archive2") hasArchive2 = true;
        }
        REQUIRE(hasArchive1);
        REQUIRE(hasArchive2);

        // Mark one as indexed
        db.MarkArchiveIndexed("archive1");

        // Now only archive2 should be new
        newArchives = indexer.GetNewArchives(db, inpxPath);
        REQUIRE(newArchives.size() == 1);
        REQUIRE(newArchives[0] == "archive2");
        
        // Mark both
        db.MarkArchiveIndexed("archive2");
        newArchives = indexer.GetNewArchives(db, inpxPath);
        REQUIRE(newArchives.empty());
    }
}

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
