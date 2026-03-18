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
        
        // Add a book with specific author and series for complex searches
        Inpx::SBookRecord r;
        r.libId = "4";
        r.title = "War and Peace";
        r.language = "ru";
        r.genres = {"prose"};
        r.rating = 5;
        r.archiveName = "arch1";
        r.series = "Classics";
        r.seriesNumber = 1;
        r.authors.push_back({"Tolstoy", "Leo", ""});
        (void)db.InsertBook(r);

        SECTION("Filter by language")
        {
            Query::SQueryParams p; 
            p.language = "ru";
            auto res = Query::CBookQuery::Execute(db, p);
            REQUIRE(res.totalFound == 3); // 1, 2, 4
            REQUIRE(res.books.size() == 3);
        }

        SECTION("Filter by genre")
        {
            Query::SQueryParams p; 
            p.genre = "sf";
            auto res = Query::CBookQuery::Execute(db, p);
            REQUIRE(res.totalFound == 2);
        }

        SECTION("Filter by author")
        {
            Query::SQueryParams p; 
            p.author = "Tolstoy";
            auto res = Query::CBookQuery::Execute(db, p);
            REQUIRE(res.totalFound == 1);
            REQUIRE(res.books[0].libId == "4");
        }

        SECTION("Filter by title")
        {
            Query::SQueryParams p; 
            p.title = "War"; // Partial match
            auto res = Query::CBookQuery::Execute(db, p);
            REQUIRE(res.totalFound == 1);
            REQUIRE(res.books[0].title == "War and Peace");
        }

        SECTION("Filter by series")
        {
            Query::SQueryParams p; 
            p.series = "Classics";
            auto res = Query::CBookQuery::Execute(db, p);
            REQUIRE(res.totalFound == 1);
            REQUIRE(res.books[0].series == "Classics");
        }

        SECTION("Combined filter (author + genre)")
        {
            Query::SQueryParams p; 
            p.author = "Tolstoy";
            p.genre = "prose";
            auto res = Query::CBookQuery::Execute(db, p);
            REQUIRE(res.totalFound == 1);
            
            p.genre = "sf"; // Tolstoy didn't write sf here
            res = Query::CBookQuery::Execute(db, p);
            REQUIRE(res.totalFound == 0);
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
            p.limit = 2;
            auto res = Query::CBookQuery::Execute(db, p);
            REQUIRE(res.totalFound == 4);
            REQUIRE(res.books.size() == 2);
        }
    }

    std::filesystem::remove(dbPath);
}
