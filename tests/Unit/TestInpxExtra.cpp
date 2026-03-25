#include <catch2/catch_test_macros.hpp>

#include "Inpx/BookRecord.hpp"
#include "Inpx/InpParser.hpp"
#include "TestUtils.hpp"

#include <filesystem>
#include <sstream>

using namespace Librium::Inpx;
using namespace Librium::Tests;

namespace {

// Produces a 3-line INP file: 2 valid books + 1 deleted
std::string MakeStreamingInpContent()
{
    constexpr char SEP = '\x04';
    std::ostringstream ss;

    auto line = [&](const std::string& authors, const std::string& genres,
                    const std::string& title, const std::string& fileId,
                    const std::string& deleted, const std::string& keywords = "")
    {
        ss << authors << SEP << genres << SEP << title << SEP
           << "" << SEP << "" << SEP
           << fileId << SEP << "512000" << SEP << fileId << SEP
           << deleted << SEP << "fb2" << SEP
           << "2021-01-01" << SEP << "en" << SEP
           << "4" << SEP << keywords << SEP << "" << "\r\n";
    };

    line("King,Stephen,:",   "thriller:", "It",              "300001", "0", "horror,clown");
    line("Tolkien,J.R.R.,:", "fantasy:",  "The Hobbit",      "300002", "0");
    line("Ghost,Author,:",   "unknown:",  "Deleted Book",    "300003", "1"); // deleted
    return ss.str();
}

} // namespace

// ---------------------------------------------------------------------------
// SAuthor tests
// ---------------------------------------------------------------------------

TEST_CASE("SAuthor::IsEmpty", "[inpx]")
{
    SECTION("Author with no fields is empty")
    {
        SAuthor a;
        REQUIRE(a.IsEmpty());
    }

    SECTION("Author with only lastName is not empty")
    {
        SAuthor a;
        a.lastName = "King";
        REQUIRE_FALSE(a.IsEmpty());
    }

    SECTION("Author with only firstName is not empty")
    {
        SAuthor a;
        a.firstName = "Stephen";
        REQUIRE_FALSE(a.IsEmpty());
    }

    SECTION("Author with only middleName is still empty (lastName+firstName check)")
    {
        SAuthor a;
        a.middleName = "Jr.";
        REQUIRE(a.IsEmpty()); // IsEmpty checks lastName and firstName only
    }

    SECTION("Fully populated author is not empty")
    {
        SAuthor a;
        a.lastName   = "Толстой";
        a.firstName  = "Лев";
        a.middleName = "Николаевич";
        REQUIRE_FALSE(a.IsEmpty());
    }
}

// ---------------------------------------------------------------------------
// SBookRecord::FilePath tests
// ---------------------------------------------------------------------------

TEST_CASE("SBookRecord::FilePath", "[inpx]")
{
    SECTION("Combines fileName and fileExt with dot")
    {
        SBookRecord r;
        r.fileName = "100001";
        r.fileExt  = "fb2";
        REQUIRE(r.FilePath() == "100001.fb2");
    }

    SECTION("Default extension is fb2")
    {
        SBookRecord r;
        r.fileName = "99999";
        REQUIRE(r.FilePath() == "99999.fb2");
    }

    SECTION("Non-fb2 extension is preserved")
    {
        SBookRecord r;
        r.fileName = "archive";
        r.fileExt  = "epub";
        REQUIRE(r.FilePath() == "archive.epub");
    }
}

// ---------------------------------------------------------------------------
// CInpParser::ParseStreaming tests
// ---------------------------------------------------------------------------

TEST_CASE("CInpParser streaming parse", "[inpx]")
{
    CTempDir tempDir;
    std::filesystem::path inpxPath = tempDir.GetPath() / "stream_test.inpx";

    CreateTestZip(inpxPath, {
        {"fb2-stream-001.zip.inp", MakeStreamingInpContent()},
        {"collection.info", "Test Library\nstream_test\n65536\nTest\n"},
        {"version.info", "20240101\r\n"}
    });

    auto u8path = inpxPath.u8string();
    auto pathStr = std::string(u8path.begin(), u8path.end());

    SECTION("Visits all non-deleted books")
    {
        CInpParser parser;
        int count = 0;

        auto stats = parser.ParseStreaming(pathStr, [&](SBookRecord&&)
        {
            ++count;
            return true;
        });

        REQUIRE(count == 2);
        REQUIRE(stats.parsedOk == 2);
        REQUIRE(stats.skippedDeleted == 1);
        REQUIRE(stats.totalLines == 3);
    }

    SECTION("Callback receives correct book data")
    {
        CInpParser parser;
        std::vector<std::string> titles;

        (void)parser.ParseStreaming(pathStr, [&](SBookRecord&& rec)
        {
            titles.push_back(rec.title);
            return true;
        });

        REQUIRE(titles.size() == 2);
        bool hasIt      = std::find(titles.begin(), titles.end(), "It")         != titles.end();
        bool hasHobbit  = std::find(titles.begin(), titles.end(), "The Hobbit") != titles.end();
        REQUIRE(hasIt);
        REQUIRE(hasHobbit);
    }

    SECTION("Early exit on callback returning false")
    {
        CInpParser parser;
        int count = 0;

        (void)parser.ParseStreaming(pathStr, [&](SBookRecord&&)
        {
            ++count;
            return false; // stop after first book
        });

        REQUIRE(count == 1);
    }

    SECTION("LastStats() reflects streaming results")
    {
        CInpParser parser;
        (void)parser.ParseStreaming(pathStr, [](SBookRecord&&) { return true; });

        REQUIRE(parser.LastStats().totalLines   == 3);
        REQUIRE(parser.LastStats().parsedOk     == 2);
        REQUIRE(parser.LastStats().skippedDeleted == 1);
    }
}

// ---------------------------------------------------------------------------
// CInpParser::CountLines tests
// ---------------------------------------------------------------------------

TEST_CASE("CInpParser::CountLines", "[inpx]")
{
    CTempDir tempDir;
    std::filesystem::path inpxPath = tempDir.GetPath() / "count_test.inpx";

    CreateTestZip(inpxPath, {
        {"fb2-count-001.zip.inp", MakeStreamingInpContent()},
        {"collection.info", "Test Library\ncount_test\n65536\nTest\n"},
        {"version.info", "20240101\r\n"}
    });

    auto u8pathCount = inpxPath.u8string();
    auto pathStr = std::string(u8pathCount.begin(), u8pathCount.end());

    SECTION("CountLines counts all lines including deleted")
    {
        size_t lines = CInpParser::CountLines(pathStr);
        REQUIRE(lines == 3);
    }

    SECTION("CountLines matches Parse totalLines stat")
    {
        size_t countLinesResult = CInpParser::CountLines(pathStr);

        CInpParser parser;
        (void)parser.Parse(pathStr);

        REQUIRE(countLinesResult == parser.LastStats().totalLines);
    }
}
