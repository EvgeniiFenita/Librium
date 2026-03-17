#include <catch2/catch_test_macros.hpp>

#include "Query/BookQuery.hpp"
#include "Database/Database.hpp"
#include <filesystem>

using namespace Librium;

namespace {

void AddBook(Db::CDatabase& db, const std::string& id, const std::string& title, const std::string& lang, const std::string& genre, int rating, const std::string& annotation = "")
{
    Inpx::SBookRecord r;
    r.libId = id;
    r.title = title;
    r.language = lang;
    r.genres = {genre};
    r.rating = rating;
    r.archiveName = "arch1";
    r.authors.push_back({"Author" + id, "First", ""});
    Fb2::SFb2Data fb2; 
    fb2.annotation = annotation;
    (void)db.InsertBook(r, fb2);
}

} // namespace

TEST_CASE("BookQuery basic execution", "[query]")
{
    std::string dbPath = "test_query.db";
    if (std::filesystem::exists(dbPath)) std::filesystem::remove(dbPath);

    {
        Db::CDatabase db(dbPath);
        AddBook(db, "1", "Book One", "ru", "sf", 5);
        AddBook(db, "2", "Book Two", "ru", "detective", 4);
        AddBook(db, "3", "The Book", "en", "sf", 3, "Some desc");

        SECTION("Filter by language")
        {
            Query::SQueryParams p; 
            p.language = "ru";
            auto res = Query::CBookQuery::Execute(db, p);
            REQUIRE(res.totalFound == 2);
            REQUIRE(res.books.size() == 2);
        }

        SECTION("Filter by genre")
        {
            Query::SQueryParams p; 
            p.genre = "sf";
            auto res = Query::CBookQuery::Execute(db, p);
            REQUIRE(res.totalFound == 2);
        }

        SECTION("Filter with annotation")
        {
            Query::SQueryParams p; 
            p.withAnnotation = true;
            auto res = Query::CBookQuery::Execute(db, p);
            REQUIRE(res.totalFound == 1);
            REQUIRE(res.books[0].libId == "3");
        }

        SECTION("Limit and offset")
        {
            Query::SQueryParams p; 
            p.limit = 1;
            auto res = Query::CBookQuery::Execute(db, p);
            REQUIRE(res.totalFound == 3);
            REQUIRE(res.books.size() == 1);
        }
    }

    std::filesystem::remove(dbPath);
}
