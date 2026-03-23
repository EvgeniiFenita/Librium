#include <catch2/catch_test_macros.hpp>

#include "Database/Database.hpp"
#include "TestUtils.hpp"
#include <filesystem>

using namespace Librium;
using namespace Librium::Tests;

namespace {

Inpx::SBookRecord MakeRec(const std::string& id="100001", const std::string& title="Test")
{
    Inpx::SBookRecord r;
    r.libId = id;
    r.title = title;
    r.archiveName = "arch1";
    r.fileName = "file1";
    r.authors.push_back({"LastName", "FirstName", ""});
    r.genres = {"sf"};
    return r;
}

} // namespace

TEST_CASE("Database basic operations", "[db]")
{
    CTempDir tempDir;
    std::string dbPath = (tempDir.GetPath() / "test.db").string();

    {
        Db::CDatabase db(dbPath);
        REQUIRE(db.CountBooks() == 0);

        auto rec = MakeRec("100001", "Test Book");
        int64_t id = db.InsertBook(rec);
        
        SECTION("Insert and Count")
        {
            REQUIRE(id > 0);
            REQUIRE(db.CountBooks() == 1);
            REQUIRE(db.CountAuthors() == 1);

            REQUIRE(db.BookExists("100001", "arch1"));
            REQUIRE_FALSE(db.BookExists("999", "arch1"));
        }

        SECTION("GetBookPath")
        {
            auto pathOpt = db.GetBookPath(id);
            REQUIRE(pathOpt.has_value());
            REQUIRE(pathOpt->archiveName == "arch1");
            REQUIRE(pathOpt->fileName == "file1.fb2");

            auto missingPath = db.GetBookPath(9999);
            REQUIRE_FALSE(missingPath.has_value());
        }

        SECTION("Archive indexing")
        {
            REQUIRE(db.GetIndexedArchives().empty());

            db.MarkArchiveIndexed("my_archive");
            
            auto archives = db.GetIndexedArchives();
            REQUIRE(archives.size() == 1);
            REQUIRE(archives[0] == "my_archive");
        }
    }
}

