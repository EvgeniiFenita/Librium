#include <catch2/catch_test_macros.hpp>

#include "Database/Database.hpp"
#include "Database/QueryTypes.hpp"
#include "Fb2/Fb2Parser.hpp"
#include "TestUtils.hpp"

#include <filesystem>

using namespace Librium;
using namespace Librium::Tests;

namespace {

void AddBookEdge(Db::CDatabase& db,
                 const std::string& id,
                 const std::string& title,
                 const std::string& lang,
                 const std::string& genre,
                 int                rating,
                 const std::string& date,
                 const std::string& annotation = "")
{
    Inpx::SBookRecord r;
    r.libId       = id;
    r.title       = title;
    r.language    = lang;
    r.genres      = {genre};
    r.rating      = rating;
    r.dateAdded   = date;
    r.archiveName = "arch1";
    r.authors.push_back({"EdgeAuthor" + id, "First", ""});

    Fb2::SFb2Data fb2;
    fb2.annotation = annotation;
    (void)db.InsertBook(r, fb2);
}

} // namespace

TEST_CASE("BookQuery rating filter", "[query]")
{
    CTempDir tempDir;
    std::string dbPath = (tempDir.GetPath() / "test_q_rating.db").string();

    {
        Db::CDatabase db(dbPath);
        AddBookEdge(db, "1", "Low",    "ru", "sf",    1, "2020-01-01");
        AddBookEdge(db, "2", "Mid",    "ru", "sf",    3, "2020-01-01");
        AddBookEdge(db, "3", "High",   "ru", "sf",    5, "2020-01-01");
        AddBookEdge(db, "4", "Top",    "ru", "prose", 5, "2020-01-01");

        SECTION("ratingMin=1 returns all books")
        {
            Db::SQueryParams p;
            p.ratingMin = 1;
            auto res = db.ExecuteQuery(p);
            REQUIRE(res.totalFound == 4);
        }

        SECTION("ratingMin=3 returns books with rating >= 3")
        {
            Db::SQueryParams p;
            p.ratingMin = 3;
            auto res = db.ExecuteQuery(p);
            REQUIRE(res.totalFound == 3); // Mid(3), High(5), Top(5)
            for (const auto& b : res.books)
                REQUIRE(b.rating >= 3);
        }

        SECTION("ratingMin=5 returns only top-rated books")
        {
            Db::SQueryParams p;
            p.ratingMin = 5;
            auto res = db.ExecuteQuery(p);
            REQUIRE(res.totalFound == 2);
            for (const auto& b : res.books)
                REQUIRE(b.rating == 5);
        }

        SECTION("ratingMin=0 or default returns all")
        {
            Db::SQueryParams p;
            // ratingMin defaults to 0
            auto res = db.ExecuteQuery(p);
            REQUIRE(res.totalFound == 4);
        }
    }
}

TEST_CASE("BookQuery date filters", "[query]")
{
    CTempDir tempDir;
    std::string dbPath = (tempDir.GetPath() / "test_q_date.db").string();

    {
        Db::CDatabase db(dbPath);
        AddBookEdge(db, "1", "Old",  "ru", "sf", 3, "2010-05-01");
        AddBookEdge(db, "2", "Mid",  "ru", "sf", 3, "2015-08-15");
        AddBookEdge(db, "3", "New",  "ru", "sf", 3, "2020-11-30");
        AddBookEdge(db, "4", "Newest","ru","sf", 3, "2022-06-01");

        SECTION("dateFrom filters out older books")
        {
            Db::SQueryParams p;
            p.dateFrom = "2016-01-01";
            auto res = db.ExecuteQuery(p);
            REQUIRE(res.totalFound == 2); // New, Newest
        }

        SECTION("dateTo filters out newer books")
        {
            Db::SQueryParams p;
            p.dateTo = "2015-12-31";
            auto res = db.ExecuteQuery(p);
            REQUIRE(res.totalFound == 2); // Old, Mid
        }

        SECTION("dateFrom + dateTo forms an inclusive range")
        {
            Db::SQueryParams p;
            p.dateFrom = "2014-01-01";
            p.dateTo   = "2021-01-01";
            auto res = db.ExecuteQuery(p);
            REQUIRE(res.totalFound == 2); // Mid, New
        }

        SECTION("dateFrom == dateTo returns books on that exact day")
        {
            Db::SQueryParams p;
            p.dateFrom = "2015-08-15";
            p.dateTo   = "2015-08-15";
            auto res = db.ExecuteQuery(p);
            REQUIRE(res.totalFound == 1);
            REQUIRE(res.books[0].libId == "2");
        }
    }
}

TEST_CASE("BookQuery empty result", "[query]")
{
    CTempDir tempDir;
    std::string dbPath = (tempDir.GetPath() / "test_q_empty.db").string();

    {
        Db::CDatabase db(dbPath);
        AddBookEdge(db, "1", "Only Book", "ru", "sf", 3, "2020-01-01");

        SECTION("Non-matching title gives empty result")
        {
            Db::SQueryParams p;
            p.title = "NoSuchTitleXYZ_12345";
            auto res = db.ExecuteQuery(p);
            REQUIRE(res.totalFound == 0);
            REQUIRE(res.books.empty());
        }

        SECTION("Query on empty database")
        {
            CTempDir innerTempDir;
            std::string emptyDb = (innerTempDir.GetPath() / "test_q_truly_empty.db").string();
            {
                Db::CDatabase emptyDbObj(emptyDb);
                Db::SQueryParams p;
                auto res = emptyDbObj.ExecuteQuery(p);
                REQUIRE(res.totalFound == 0);
                REQUIRE(res.books.empty());
            }
        }
    }
}

TEST_CASE("BookQuery multi-author books", "[query]")
{
    CTempDir tempDir;
    std::string dbPath = (tempDir.GetPath() / "test_q_multiauthor.db").string();

    {
        Db::CDatabase db(dbPath);

        // Single-author book
        AddBookEdge(db, "1", "Solo Work", "en", "sf", 4, "2020-01-01");

        // Multi-author book
        Inpx::SBookRecord multi;
        multi.libId       = "2";
        multi.title       = "Joint Venture";
        multi.language    = "en";
        multi.genres      = {"sf"};
        multi.rating      = 5;
        multi.dateAdded   = "2021-01-01";
        multi.archiveName = "arch1";
        multi.authors.push_back({"Smith",   "Alice", ""});
        multi.authors.push_back({"Johnson", "Bob",   ""});
        (void)db.InsertBook(multi);

        SECTION("Book found by first author")
        {
            Db::SQueryParams p;
            p.author = "Smith";
            auto res = db.ExecuteQuery(p);
            REQUIRE(res.totalFound == 1);
            REQUIRE(res.books[0].title == "Joint Venture");
        }

        SECTION("Book found by second author")
        {
            Db::SQueryParams p;
            p.author = "Johnson";
            auto res = db.ExecuteQuery(p);
            REQUIRE(res.totalFound == 1);
            REQUIRE(res.books[0].title == "Joint Venture");
        }

        SECTION("Result includes all authors of multi-author book")
        {
            Db::SQueryParams p;
            p.author = "Smith";
            auto res = db.ExecuteQuery(p);
            REQUIRE(res.totalFound == 1);
            REQUIRE(res.books[0].authors.size() == 2);
        }

        SECTION("Single-author book found independently")
        {
            Db::SQueryParams p;
            p.title = "Solo";
            auto res = db.ExecuteQuery(p);
            REQUIRE(res.totalFound == 1);
            REQUIRE(res.books[0].authors.size() == 1);
        }
    }
}

TEST_CASE("BookQuery pagination offset", "[query]")
{
    CTempDir tempDir;
    std::string dbPath = (tempDir.GetPath() / "test_q_pagination.db").string();

    {
        Db::CDatabase db(dbPath);
        for (int i = 1; i <= 6; ++i)
            AddBookEdge(db, std::to_string(i), "Book " + std::to_string(i),
                        "ru", "sf", 3, "2020-01-01");

        SECTION("First page has correct size")
        {
            Db::SQueryParams p;
            p.limit  = 2;
            p.offset = 0;
            auto res = db.ExecuteQuery(p);
            REQUIRE(res.totalFound == 6);
            REQUIRE(res.books.size() == 2);
        }

        SECTION("Second page returns next items")
        {
            Db::SQueryParams p;
            p.limit  = 2;
            p.offset = 0;
            auto page1 = db.ExecuteQuery(p);

            p.offset = 2;
            auto page2 = db.ExecuteQuery(p);

            REQUIRE(page2.totalFound == 6);
            REQUIRE(page2.books.size() == 2);
            // Pages should not overlap
            REQUIRE(page1.books[0].libId != page2.books[0].libId);
            REQUIRE(page1.books[1].libId != page2.books[1].libId);
        }

        SECTION("Offset beyond total returns empty books list")
        {
            Db::SQueryParams p;
            p.limit  = 10;
            p.offset = 100;
            auto res = db.ExecuteQuery(p);
            REQUIRE(res.totalFound == 6); // total is still correct
            REQUIRE(res.books.empty());
        }

        SECTION("Three full pages cover all items exactly")
        {
            size_t total = 0;
            for (int page = 0; page < 3; ++page)
            {
                Db::SQueryParams p;
                p.limit  = 2;
                p.offset = page * 2;
                auto res = db.ExecuteQuery(p);
                total += res.books.size();
            }
            REQUIRE(total == 6);
        }
    }
}

TEST_CASE("CDatabase::GetBookById", "[db]")
{
    CTempDir tempDir;
    std::string dbPath = (tempDir.GetPath() / "test_get_by_id.db").string();

    {
        Db::CDatabase db(dbPath);

        Inpx::SBookRecord r;
        r.libId       = "42";
        r.title       = "Find Me";
        r.language    = "en";
        r.genres      = {"sf"};
        r.archiveName = "arch1";
        r.authors.push_back({"Finder", "Test", ""});
        int64_t id = db.InsertBook(r);

        SECTION("Returns book for valid id")
        {
            auto result = db.GetBookById(id);
            REQUIRE(result.has_value());
            REQUIRE(result->title == "Find Me");
            REQUIRE(result->libId == "42");
        }

        SECTION("Returns nullopt for non-existent id")
        {
            auto result = db.GetBookById(99999);
            REQUIRE_FALSE(result.has_value());
        }

        SECTION("Returned book has correct author")
        {
            auto result = db.GetBookById(id);
            REQUIRE(result.has_value());
            REQUIRE(result->authors.size() == 1);
            REQUIRE(result->authors[0].lastName == "Finder");
        }
    }
}

