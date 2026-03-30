#include <catch2/catch_test_macros.hpp>

#include "Database/Database.hpp"
#include "Database/QueryTypes.hpp"
#include "Fb2/Fb2Parser.hpp"
#include "Inpx/InpParser.hpp"
#include "Utils/StringUtils.hpp"
#include "TestUtils.hpp"

#include <filesystem>

using namespace Librium;
using namespace Librium::Tests;

// NOTE: Basic GetBookById tests (found/not-found/author fields) are already in
// TestQueryEdgeCases.cpp. This file tests FB2-derived fields stored by InsertBook.

namespace {

int64_t InsertFullBook(Db::CDatabase& db,
                       const std::string& libId,
                       const std::string& title,
                       const std::string& annotation,
                       const std::string& publisher,
                       const std::string& isbn,
                       const std::string& publishYear)
{
    Inpx::SBookRecord r;
    r.libId       = libId;
    r.title       = title;
    r.archiveName = "arch_full";
    r.authors.push_back({"Smith", "John", ""});
    r.genres      = {"sf", "detective"};
    r.series      = "Galaxy";
    r.seriesNumber = 3;
    r.language    = "en";
    r.rating      = 7;

    Fb2::SFb2Data fb2;
    fb2.annotation  = annotation;
    fb2.publisher   = publisher;
    fb2.isbn        = isbn;
    fb2.publishYear = publishYear;

    return db.InsertBook(r, fb2);
}

} // namespace

TEST_CASE("GetBookById: FB2 annotation is stored and retrieved", "[db][getbook]")
{
    CTempDir tempDir;
    std::string dbPath = (tempDir.GetPath() / "test_getbook_fb2.db").string();

    {
        Db::CDatabase db(dbPath);
        int64_t id = InsertFullBook(db, "A001", "Annotated Title",
                                    "Detailed annotation text.", "Pub House",
                                    "978-0-1234-5678-9", "2021");

        auto result = db.GetBookById(id);
        REQUIRE(result.has_value());
        REQUIRE(result->annotation == "Detailed annotation text.");
    }
}

TEST_CASE("GetBookById: publisher, ISBN, publishYear are stored and retrieved", "[db][getbook]")
{
    CTempDir tempDir;
    std::string dbPath = (tempDir.GetPath() / "test_getbook_pub.db").string();

    {
        Db::CDatabase db(dbPath);
        int64_t id = InsertFullBook(db, "B001", "Publisher Book",
                                    "Some annotation.", "Galaxy Press",
                                    "978-3-1614-5153-2", "2023");

        auto result = db.GetBookById(id);
        REQUIRE(result.has_value());
        REQUIRE(result->publisher    == "Galaxy Press");
        REQUIRE(result->isbn         == "978-3-1614-5153-2");
        REQUIRE(result->publishYear  == "2023");
        REQUIRE(result->annotation   == "Some annotation.");
    }
}

TEST_CASE("GetBookById: multiple genres are all returned", "[db][getbook]")
{
    CTempDir tempDir;
    std::string dbPath = (tempDir.GetPath() / "test_getbook_genres.db").string();

    {
        Db::CDatabase db(dbPath);
        int64_t id = InsertFullBook(db, "C001", "Multi-Genre Book",
                                    "", "", "", "");

        auto result = db.GetBookById(id);
        REQUIRE(result.has_value());
        REQUIRE(result->genres.size() == 2);
        bool hasSf       = std::find(result->genres.begin(), result->genres.end(), "Science Fiction") != result->genres.end();
        bool hasDetective = std::find(result->genres.begin(), result->genres.end(), "Detective") != result->genres.end();
        REQUIRE(hasSf);
        REQUIRE(hasDetective);
    }
}

TEST_CASE("GetBookById: series number is stored and retrieved", "[db][getbook]")
{
    CTempDir tempDir;
    std::string dbPath = (tempDir.GetPath() / "test_getbook_series.db").string();

    {
        Db::CDatabase db(dbPath);
        int64_t id = InsertFullBook(db, "D001", "Series Book", "", "", "", "");

        auto result = db.GetBookById(id);
        REQUIRE(result.has_value());
        REQUIRE(result->series       == "Galaxy");
        REQUIRE(result->seriesNumber == 3);
    }
}

TEST_CASE("GetBookById: book with empty FB2 fields has empty strings", "[db][getbook]")
{
    CTempDir tempDir;
    std::string dbPath = (tempDir.GetPath() / "test_getbook_empty_fb2.db").string();

    {
        Db::CDatabase db(dbPath);

        Inpx::SBookRecord r;
        r.libId       = "E001";
        r.title       = "No FB2 Data";
        r.archiveName = "arch1";
        r.authors.push_back({"Solo", "Author", ""});
        int64_t id = db.InsertBook(r); // no fb2 data

        auto result = db.GetBookById(id);
        REQUIRE(result.has_value());
        REQUIRE(result->annotation.empty());
        REQUIRE(result->publisher.empty());
        REQUIRE(result->isbn.empty());
        REQUIRE(result->publishYear.empty());
    }
}

TEST_CASE("GetBookById: very long annotation is stored without truncation", "[db][getbook]")
{
    CTempDir tempDir;
    std::string dbPath = (tempDir.GetPath() / "test_getbook_long_ann.db").string();

    {
        Db::CDatabase db(dbPath);
        std::string longAnnotation(10000, 'X');
        int64_t id = InsertFullBook(db, "F001", "Long Annotation Book",
                                    longAnnotation, "", "", "");

        auto result = db.GetBookById(id);
        REQUIRE(result.has_value());
        REQUIRE(result->annotation.size() == 10000);
        REQUIRE(result->annotation == longAnnotation);
    }
}

TEST_CASE("GetBookById: genre is translated from INPX short code to English", "[db][getbook][genre]")
{
    CTempDir tempDir;
    std::string dbPath = (tempDir.GetPath() / "test_getbook_genre_translation.db").string();
    std::filesystem::path inpxPath = tempDir.GetPath() / "genre_translation.inpx";

    // AUTHORS\x04GENRES\x04TITLE\x04SERIES\x04SERNO\x04FILEID\x04FILESIZE\x04LIBID\x04DEL\x04EXT\x04DATE\x04LANG\x04RATING\x04KEYWORDS
    std::string content = "Author,Test,:\x04" "sf_history:prose:\x04" "Translation Test\x04\x04\x04" "800001\x04" "1000\x04" "0\x04" "0\x04" "fb2\x04" "2023-01-01\x04" "en\x04" "0\x04\x04\r\n";

    CreateTestZip(inpxPath, {
        {"fb2-trans.zip.inp", content},
        {"collection.info", "Translation Test\ntr\n65536\n"},
        {"version.info", "20240101\r\n"}
    });

    Inpx::CInpParser parser;
    auto books = parser.Parse(Librium::Utils::CStringUtils::PathToUtf8String(inpxPath));
    REQUIRE(books.size() == 1);
    
    // Inpx parser leaves short codes unchanged
    REQUIRE(books[0].genres.size() == 2);
    REQUIRE(books[0].genres[0] == "sf_history");
    REQUIRE(books[0].genres[1] == "prose");

    {
        Db::CDatabase db(dbPath);
        int64_t id = db.InsertBook(books[0]);
        REQUIRE(id > 0);

        auto result = db.GetBookById(id);
        REQUIRE(result.has_value());
        REQUIRE(result->genres.size() == 2);
        
        // Database translates short codes to English
        bool hasAltHistory = std::find(result->genres.begin(), result->genres.end(), "Alternative History") != result->genres.end();
        bool hasProse      = std::find(result->genres.begin(), result->genres.end(), "Prose") != result->genres.end();
        REQUIRE(hasAltHistory);
        REQUIRE(hasProse);
    }
}
