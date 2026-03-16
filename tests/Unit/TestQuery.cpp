#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include "Database/Database.hpp"
#include "Fb2/Fb2Parser.hpp"
#include "Inpx/BookRecord.hpp"
#include "Query/BookQuery.hpp"
#include "Query/QuerySerializer.hpp"

using namespace Librium;
using namespace Librium::Query;

namespace {
void InsertBooks(Db::CDatabase& db) 
{
    auto book = [&](const std::string& id, const std::string& title,
                    const std::string& lang, const std::string& genre,
                    int rating, const std::string& annotation = "") 
{
        Inpx::CBookRecord r;
        r.libId = id; r.archiveName = "test.zip";
        r.title = title; r.language = lang; r.rating = rating;
        r.fileName = id; r.fileExt = "fb2"; r.fileSize = 100000;
        r.dateAdded = "2020-01-01";
        r.genres.push_back(genre);
        r.authors.push_back({"CAuthor" + id, "First", ""});
        Fb2::CFb2Data fb2; fb2.annotation = annotation;
        (void)db.InsertBook(r, fb2);
    };
    book("1", "Russian Novel",   "ru", "prose_classic", 5, "Great annotation");
    book("2", "English Thriller","en", "thriller",      4);
    book("3", "Russian SF",      "ru", "sf",            3);
}
} // namespace

TEST_CASE("Empty DB returns empty result", "[query]") 
{
    Db::CDatabase db(":memory:");
    REQUIRE(CBookQuery::Execute(db, {}).totalFound == 0);
}
TEST_CASE("Query all finds all books", "[query]") 
{
    Db::CDatabase db(":memory:");
    InsertBooks(db);
    REQUIRE(CBookQuery::Execute(db, {}).totalFound == 3);
}
TEST_CASE("Filter by language", "[query]") 
{
    Db::CDatabase db(":memory:");
    InsertBooks(db);
    QueryParams p; p.language = "ru";
    auto r = CBookQuery::Execute(db, p);
    REQUIRE(r.totalFound == 2);
    for (const auto& b : r.books) REQUIRE(b.language == "ru");
}
TEST_CASE("Filter by genre", "[query]") 
{
    Db::CDatabase db(":memory:");
    InsertBooks(db);
    QueryParams p; p.genre = "thriller";
    REQUIRE(CBookQuery::Execute(db, p).totalFound == 1);
}
TEST_CASE("withAnnotation filter", "[query]") 
{
    Db::CDatabase db(":memory:");
    InsertBooks(db);
    QueryParams p; p.withAnnotation = true;
    auto r = CBookQuery::Execute(db, p);
    REQUIRE(r.totalFound == 1);
    REQUIRE_FALSE(r.books[0].annotation.empty());
}
TEST_CASE("Limit and totalFound", "[query]") 
{
    Db::CDatabase db(":memory:");
    InsertBooks(db);
    QueryParams p; p.limit = 1;
    auto r = CBookQuery::Execute(db, p);
    REQUIRE(r.books.size() == 1);
    REQUIRE(r.totalFound == 3);
}
TEST_CASE("ToJson has required fields", "[query]") 
{
    Db::CDatabase db(":memory:");
    InsertBooks(db);
    auto r = CBookQuery::Execute(db, {});
    auto j = CQuerySerializer::ToJson(r);
    REQUIRE(j.contains("totalFound"));
    REQUIRE(j.contains("books"));
    REQUIRE(j.contains("query"));
    for (const auto& b : j["books"]) 
{
        REQUIRE(b.contains("libId"));
        REQUIRE(b.contains("title"));
        REQUIRE_FALSE(b.contains("cover"));
    }
}
TEST_CASE("SaveToFile writes JSON", "[query]") 
{
    Db::CDatabase db(":memory:");
    InsertBooks(db);
    const std::string path = "test_query_out.json";
    std::filesystem::remove(path);
    CQuerySerializer::SaveToFile(CBookQuery::Execute(db, {}), path);
    REQUIRE(std::filesystem::exists(path));
    std::filesystem::remove(path);
}






