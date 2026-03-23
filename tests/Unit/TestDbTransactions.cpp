#include <catch2/catch_test_macros.hpp>

#include "Database/Database.hpp"
#include "TestUtils.hpp"

#include <filesystem>

using namespace Librium;
using namespace Librium::Tests;

namespace {

Inpx::SBookRecord MakeTxRec(const std::string& id, const std::string& title = "Tx Book")
{
    Inpx::SBookRecord r;
    r.libId       = id;
    r.title       = title;
    r.archiveName = "tx_arch";
    r.fileName    = id;
    r.authors.push_back({"TxAuthor", "Test", ""});
    r.genres = {"test"};
    return r;
}

} // namespace

TEST_CASE("Database transaction commit", "[db][transactions]")
{
    CTempDir tempDir;
    std::string dbPath = (tempDir.GetPath() / "test_tx_commit.db").string();

    {
        Db::CDatabase db(dbPath);

        db.BeginTransaction();
        (void)db.InsertBook(MakeTxRec("tx1", "Book One"));
        (void)db.InsertBook(MakeTxRec("tx2", "Book Two"));
        db.Commit();

        REQUIRE(db.CountBooks() == 2);
        REQUIRE(db.BookExists("tx1", "tx_arch"));
        REQUIRE(db.BookExists("tx2", "tx_arch"));
    }
}

TEST_CASE("Database transaction rollback", "[db][transactions]")
{
    CTempDir tempDir;
    std::string dbPath = (tempDir.GetPath() / "test_tx_rollback.db").string();

    {
        Db::CDatabase db(dbPath);

        db.BeginTransaction();
        (void)db.InsertBook(MakeTxRec("tx1", "Should Disappear"));
        db.Rollback();

        REQUIRE(db.CountBooks() == 0);
        REQUIRE_FALSE(db.BookExists("tx1", "tx_arch"));
    }
}

TEST_CASE("Database multiple transaction cycles", "[db][transactions]")
{
    CTempDir tempDir;
    std::string dbPath = (tempDir.GetPath() / "test_tx_cycles.db").string();

    {
        Db::CDatabase db(dbPath);

        // First cycle: commit
        db.BeginTransaction();
        (void)db.InsertBook(MakeTxRec("kept1", "Kept One"));
        db.Commit();

        // Second cycle: rollback
        db.BeginTransaction();
        (void)db.InsertBook(MakeTxRec("lost1", "Lost One"));
        db.Rollback();

        // Third cycle: commit
        db.BeginTransaction();
        (void)db.InsertBook(MakeTxRec("kept2", "Kept Two"));
        db.Commit();

        REQUIRE(db.CountBooks() == 2);
        REQUIRE(db.BookExists("kept1", "tx_arch"));
        REQUIRE(db.BookExists("kept2", "tx_arch"));
        REQUIRE_FALSE(db.BookExists("lost1", "tx_arch"));
    }
}

TEST_CASE("Database transaction preserves count integrity", "[db][transactions]")
{
    CTempDir tempDir;
    std::string dbPath = (tempDir.GetPath() / "test_tx_integrity.db").string();

    {
        Db::CDatabase db(dbPath);

        // Insert one book outside a transaction (auto-commit)
        (void)db.InsertBook(MakeTxRec("base1", "Base Book"));
        REQUIRE(db.CountBooks() == 1);

        // Rollback should not affect the already-committed book
        db.BeginTransaction();
        (void)db.InsertBook(MakeTxRec("temp1", "Temp Book"));
        REQUIRE(db.CountBooks() == 2); // visible within transaction
        db.Rollback();

        REQUIRE(db.CountBooks() == 1); // only base book remains
    }
}

