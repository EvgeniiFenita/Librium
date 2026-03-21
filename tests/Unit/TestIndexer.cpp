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

    auto countIndexes = [](Librium::Db::CDatabase& database) -> int
    {
        const std::string sql = "SELECT COUNT(*) FROM sqlite_master WHERE type='index' AND name LIKE 'idx_books_%'";
        auto stmt = database.Handle()->Prepare(sql);
        stmt->Step();
        return stmt->IsRow() ? stmt->ColumnInt(0) : -1;
    };

    // 1. Initialize DB and verify indexes exist
    CDatabase db(dbPath);
    REQUIRE(countIndexes(db) > 0);
    
    // Mark archive as indexed so upgrade mode has 0 work
    db.MarkArchiveIndexed("archive1");

    // 2. Run indexer in upgrade mode (should find 0 work)
    CIndexer indexer(cfg);
    (void)indexer.Run(db, nullptr);

    // 3. Verify indexes STILL exist (they shouldn't have been dropped)
    REQUIRE(countIndexes(db) > 0);
}
