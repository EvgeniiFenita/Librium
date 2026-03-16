#include <catch2/catch_test_macros.hpp>
#include "Database/Database.hpp"
#include "Inpx/BookRecord.hpp"

using namespace LibIndexer;

namespace {
Inpx::BookRecord MakeRec(const std::string& id="100001",
                          const std::string& arch="fb2-test-001.zip")
{
    Inpx::BookRecord r;
    r.libId = id; r.archiveName = arch;
    r.title = "Test"; r.language = "ru"; r.fileExt = "fb2";
    r.fileName = id; r.fileSize = 1024;
    r.authors.push_back({"Tolstoy","Leo","Nikolaevich"});
    r.genres.push_back("prose_classic");
    return r;
}
} // namespace

TEST_CASE("CDatabase constructs", "[db]")       { REQUIRE_NOTHROW(Db::CDatabase(":memory:")); }
TEST_CASE("InsertBook returns positive id", "[db]")
{
    Db::CDatabase db(":memory:");
    REQUIRE(db.InsertBook(MakeRec()) > 0);
}
TEST_CASE("BookExists before/after insert", "[db]")
{
    Db::CDatabase db(":memory:");
    auto r = MakeRec();
    REQUIRE_FALSE(db.BookExists(r.libId, r.archiveName));
    (void)db.InsertBook(r);
    REQUIRE(db.BookExists(r.libId, r.archiveName));
}
TEST_CASE("Duplicate insert returns 0", "[db]")
{
    Db::CDatabase db(":memory:");
    auto r = MakeRec();
    REQUIRE(db.InsertBook(r) > 0);
    REQUIRE(db.InsertBook(r) == 0);
}
TEST_CASE("CountBooks increments", "[db]")
{
    Db::CDatabase db(":memory:");
    (void)db.InsertBook(MakeRec("1")); (void)db.InsertBook(MakeRec("2"));
    REQUIRE(db.CountBooks() == 2);
}
TEST_CASE("Authors deduplicated", "[db]")
{
    Db::CDatabase db(":memory:");
    (void)db.InsertBook(MakeRec("1")); (void)db.InsertBook(MakeRec("2"));
    REQUIRE(db.CountAuthors() == 1);
}
TEST_CASE("Rollback leaves DB empty", "[db]")
{
    Db::CDatabase db(":memory:");
    db.BeginTransaction(); (void)db.InsertBook(MakeRec()); db.Rollback();
    REQUIRE(db.CountBooks() == 0);
}
