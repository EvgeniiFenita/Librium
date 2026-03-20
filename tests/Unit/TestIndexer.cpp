#include <catch2/catch_test_macros.hpp>
#include "Indexer/Indexer.hpp"
#include "Database/Database.hpp"
#include "TestUtils.hpp"

#include <sqlite3.h>
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
    cfg.import.mode = "upgrade";

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
    cfg.import.mode = "upgrade";

    auto countIndexes = [](const std::string& path) {
        sqlite3* db = nullptr;
        if (sqlite3_open(path.c_str(), &db) != SQLITE_OK) return -1;
        
        int count = 0;
        sqlite3_stmt* stmt = nullptr;
        const char* sql = "SELECT COUNT(*) FROM sqlite_master WHERE type='index' AND name LIKE 'idx_books_%'";
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            if (sqlite3_step(stmt) == SQLITE_ROW) count = sqlite3_column_int(stmt, 0);
            sqlite3_finalize(stmt);
        }
        sqlite3_close(db);
        return count;
    };

    // 1. Initialize DB and verify indexes exist
    CDatabase db(dbPath);
    REQUIRE(countIndexes(dbPath) > 0);
    
    // Mark archive as indexed so upgrade mode has 0 work
    db.MarkArchiveIndexed("archive1");

    // 2. Run indexer in upgrade mode (should find 0 work)
    CIndexer indexer(cfg);
    (void)indexer.Run(db, nullptr);

    // 3. Verify indexes STILL exist (they shouldn't have been dropped)
    // We check via raw sqlite to avoid CDatabase constructor's "auto-fix"
    REQUIRE(countIndexes(dbPath) > 0);
}
