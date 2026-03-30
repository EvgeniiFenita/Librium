#include <catch2/catch_test_macros.hpp>

#include "Database/QueryTypes.hpp"
#include "Database/Database.hpp"
#include "TestUtils.hpp"
#include <filesystem>

using namespace Librium;
using namespace Librium::Tests;

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
    CTempDir tempDir;
    std::string dbPath = (tempDir.GetPath() / "test_query.db").string();

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
            Db::SQueryParams p; 
            p.language = "ru";
            auto res = db.ExecuteQuery(p);
            REQUIRE(res.totalFound == 3); // 1, 2, 4
            REQUIRE(res.books.size() == 3);
        }

        SECTION("Filter by genre")
        {
            Db::SQueryParams p; 
            p.genre = "sf";
            auto res = db.ExecuteQuery(p);
            REQUIRE(res.totalFound == 2);
        }

        SECTION("Filter by author")
        {
            Db::SQueryParams p; 
            p.author = "Tolstoy";
            auto res = db.ExecuteQuery(p);
            REQUIRE(res.totalFound == 1);
            REQUIRE(res.books[0].libId == "4");
        }

        SECTION("Filter by title")
        {
            Db::SQueryParams p; 
            p.title = "War"; // Partial match
            auto res = db.ExecuteQuery(p);
            REQUIRE(res.totalFound == 1);
            REQUIRE(res.books[0].title == "War and Peace");
        }

        SECTION("Filter by series")
        {
            Db::SQueryParams p; 
            p.series = "Classics";
            auto res = db.ExecuteQuery(p);
            REQUIRE(res.totalFound == 1);
            REQUIRE(res.books[0].series == "Classics");
        }

        SECTION("Combined filter (author + genre)")
        {
            Db::SQueryParams p; 
            p.author = "Tolstoy";
            p.genre = "prose";
            auto res = db.ExecuteQuery(p);
            REQUIRE(res.totalFound == 1);
            
            p.genre = "sf"; // Tolstoy didn't write sf here
            res = db.ExecuteQuery(p);
            REQUIRE(res.totalFound == 0);
        }

        SECTION("Filter with annotation")
        {
            Db::SQueryParams p; 
            p.withAnnotation = true;
            auto res = db.ExecuteQuery(p);
            REQUIRE(res.totalFound == 1);
            REQUIRE(res.books[0].libId == "3");
        }

        SECTION("Limit and offset")
        {
            Db::SQueryParams p; 
            p.limit = 2;
            auto res = db.ExecuteQuery(p);
            REQUIRE(res.totalFound == 4);
            REQUIRE(res.books.size() == 2);
        }
    }
}

TEST_CASE("BookQuery: no duplicates when multiple authors match search term", "[query][dedup]")
{
    CTempDir tempDir;
    Db::CDatabase db((tempDir.GetPath() / "dedup_query.db").string());

    // One book with two authors whose search_name both start with "Александров"
    // (search_name = lastName + " " + firstName, so prefix "Александров" matches both)
    Inpx::SBookRecord r;
    r.libId       = "D1";
    r.title       = "Dedup Book";
    r.archiveName = "arch1";
    r.language    = "ru";
    r.genres      = {"sf"};
    r.authors.push_back({"Александров", "Иван",  ""});
    r.authors.push_back({"Александров", "Петр",  ""});
    (void)db.InsertBook(r, {});

    // Unrelated second book — must not appear
    Inpx::SBookRecord r2;
    r2.libId       = "D2";
    r2.title       = "Other Book";
    r2.archiveName = "arch1";
    r2.language    = "ru";
    r2.genres      = {"detective"};
    r2.authors.push_back({"Иванов", "Сергей", ""});
    (void)db.InsertBook(r2, {});

    SECTION("author prefix matching both authors of one book: totalFound==1, books.size()==1")
    {
        Db::SQueryParams p;
        p.author = "Александров"; // prefix — matches "Александров Иван" and "Александров Петр"
        auto res = db.ExecuteQuery(p);
        REQUIRE(res.totalFound == 1);
        REQUIRE(res.books.size() == 1);
        REQUIRE(res.books[0].libId == "D1");
    }

    SECTION("author + genre combined: still one result, no duplicates")
    {
        Db::SQueryParams p;
        p.author = "Александров";
        p.genre  = "sf";
        auto res = db.ExecuteQuery(p);
        REQUIRE(res.totalFound == 1);
        REQUIRE(res.books.size() == 1);
    }

    SECTION("author filter that matches nothing: zero results")
    {
        Db::SQueryParams p;
        p.author = "Достоевский";
        auto res = db.ExecuteQuery(p);
        REQUIRE(res.totalFound == 0);
        REQUIRE(res.books.empty());
    }
}

TEST_CASE("BookQuery batched hydration handles result sets larger than sqlite variable limit", "[query][batch]")
{
    CTempDir tempDir;
    Db::CDatabase db((tempDir.GetPath() / "batch_query.db").string());

    for (int i = 0; i < 1200; ++i)
    {
        Inpx::SBookRecord r;
        r.libId = std::to_string(i + 1);
        r.title = "Batch Book " + std::to_string(i + 1);
        r.archiveName = "arch1";
        r.language = "ru";
        r.genres = {"sf"};
        r.authors.push_back({"Author", std::to_string(i + 1), ""});
        (void)db.InsertBook(r, {});
    }

    Db::SQueryParams p;
    p.limit = 1200;
    auto res = db.ExecuteQuery(p);

    REQUIRE(res.totalFound == 1200);
    REQUIRE(res.books.size() == 1200);
    REQUIRE(res.books.front().authors.size() == 1);
    REQUIRE(res.books.front().genres == std::vector<std::string>{"Science Fiction"});
    REQUIRE(res.books.back().authors.size() == 1);
    REQUIRE(res.books.back().genres == std::vector<std::string>{"Science Fiction"});
}
