#include <catch2/catch_test_macros.hpp>

#include "Database/Database.hpp"
#include <filesystem>

using namespace Librium;

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
    std::string dbPath = "test.db";
    if (std::filesystem::exists(dbPath)) std::filesystem::remove(dbPath);

    {
        Db::CDatabase db(dbPath);
        REQUIRE(db.CountBooks() == 0);

        auto rec = MakeRec();
        int64_t id = db.InsertBook(rec);
        REQUIRE(id > 0);
        REQUIRE(db.CountBooks() == 1);
        REQUIRE(db.CountAuthors() == 1);

        REQUIRE(db.BookExists("100001", "arch1"));
        REQUIRE_FALSE(db.BookExists("999", "arch1"));
    }

    std::filesystem::remove(dbPath);
}
