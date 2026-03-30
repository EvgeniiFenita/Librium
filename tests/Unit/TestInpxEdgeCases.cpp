#include <catch2/catch_test_macros.hpp>

#include "Inpx/InpParser.hpp"
#include "TestUtils.hpp"

#include <filesystem>
#include <sstream>

using namespace Librium::Inpx;
using namespace Librium::Tests;

// NOTE: Single-archive INPX parsing and deleted-book filtering are already
// tested in TestInpx.cpp and TestInpxExtra.cpp. This file tests additional
// structural edge cases.

namespace {

constexpr char SEP = '\x04';

std::string MakeLine(const std::string& authors,
                     const std::string& genres,
                     const std::string& title,
                     const std::string& series    = "",
                     const std::string& serNo     = "",
                     const std::string& fileId    = "200001",
                     const std::string& size      = "512000",
                     const std::string& deleted   = "0",
                     const std::string& lang      = "ru",
                     const std::string& keywords  = "")
{
    std::ostringstream ss;
    ss << authors << SEP << genres << SEP << title << SEP
       << series  << SEP << serNo  << SEP
       << fileId  << SEP << size   << SEP << fileId << SEP
       << deleted << SEP << "fb2"  << SEP
       << "2023-06-01" << SEP << lang << SEP
       << "4" << SEP << keywords << SEP << "" << "\r\n";
    return ss.str();
}

} // namespace

TEST_CASE("CInpParser: multiple .inp files in one INPX", "[inpx][edge]")
{
    // An INPX can contain multiple .inp files representing different archives.
    // All books from all .inp files must be returned.
    CTempDir tempDir;
    std::filesystem::path inpxPath = tempDir.GetPath() / "multi_archive.inpx";

    std::string arch1 = MakeLine("Author,A,:", "sf:", "Book A1", "", "", "300001")
                      + MakeLine("Author,A,:", "sf:", "Book A2", "", "", "300002");

    std::string arch2 = MakeLine("Author,B,:", "prose:", "Book B1", "", "", "400001")
                      + MakeLine("Author,B,:", "prose:", "Book B2", "", "", "400002")
                      + MakeLine("Author,B,:", "prose:", "Book B3", "", "", "400003");

    CreateTestZip(inpxPath, {
        {"fb2-arch1.zip.inp", arch1},
        {"fb2-arch2.zip.inp", arch2},
        {"collection.info", "Multi Test\nmulti\n65536\n"},
        {"version.info", "20240101\r\n"}
    });

    auto u8path = inpxPath.u8string();
    CInpParser parser;
    auto books = parser.Parse(std::string(u8path.begin(), u8path.end()));

    REQUIRE(books.size() == 5); // 2 from arch1 + 3 from arch2

    // Verify books from BOTH archives are present, not just one repeated.
    int countA = 0, countB = 0;
    for (const auto& b : books)
    {
        if (!b.authors.empty())
        {
            if (b.authors[0].firstName == "A") ++countA;
            if (b.authors[0].firstName == "B") ++countB;
        }
    }
    REQUIRE(countA == 2); // 2 books from arch1
    REQUIRE(countB == 3); // 3 books from arch2
}

TEST_CASE("CInpParser: book with empty genres field is parsed without crash", "[inpx][edge]")
{
    CTempDir tempDir;
    std::filesystem::path inpxPath = tempDir.GetPath() / "no_genre.inpx";

    // Empty genre field (no colon-separated values)
    std::string content = MakeLine("Author,X,:", ":", "No Genre Book", "", "", "500001");

    CreateTestZip(inpxPath, {
        {"fb2-ng.zip.inp", content},
        {"collection.info", "No Genre\nng\n65536\n"},
        {"version.info", "20240101\r\n"}
    });

    auto u8path = inpxPath.u8string();
    CInpParser parser;
    REQUIRE_NOTHROW(parser.Parse(std::string(u8path.begin(), u8path.end())));
    auto books = parser.Parse(std::string(u8path.begin(), u8path.end()));
    REQUIRE(books.size() == 1);
    REQUIRE(books[0].genres.empty());
}

TEST_CASE("CInpParser: book with very long title is stored intact", "[inpx][edge]")
{
    CTempDir tempDir;
    std::filesystem::path inpxPath = tempDir.GetPath() / "long_title.inpx";

    std::string longTitle(500, 'T');
    std::string content = MakeLine("Author,Y,:", "sf:", longTitle, "", "", "600001");

    CreateTestZip(inpxPath, {
        {"fb2-lt.zip.inp", content},
        {"collection.info", "Long Title\nlt\n65536\n"},
        {"version.info", "20240101\r\n"}
    });

    auto u8path = inpxPath.u8string();
    CInpParser parser;
    auto books = parser.Parse(std::string(u8path.begin(), u8path.end()));

    REQUIRE(books.size() == 1);
    REQUIRE(books[0].title.size() == 500);
    REQUIRE(books[0].title == longTitle);
}

TEST_CASE("CInpParser: book with empty title is parsed without crash", "[inpx][edge]")
{
    CTempDir tempDir;
    std::filesystem::path inpxPath = tempDir.GetPath() / "empty_title.inpx";

    std::string content = MakeLine("Author,Z,:", "sf:", "", "", "", "700001");

    CreateTestZip(inpxPath, {
        {"fb2-et.zip.inp", content},
        {"collection.info", "Empty Title\net\n65536\n"},
        {"version.info", "20240101\r\n"}
    });

    auto u8path = inpxPath.u8string();
    CInpParser parser;
    // Empty title is allowed if fileName is non-empty — book is still parsed
    auto books = parser.Parse(std::string(u8path.begin(), u8path.end()));
    REQUIRE(books.size() == 1);
    REQUIRE(books[0].title.empty());
    REQUIRE(books[0].authors[0].lastName == "Author");
}

TEST_CASE("CInpParser: keywords field is correctly parsed", "[inpx][edge]")
{
    CTempDir tempDir;
    std::filesystem::path inpxPath = tempDir.GetPath() / "keywords.inpx";

    std::string content = MakeLine("Keyw,Test,:", "sf:", "Keyword Book",
                                   "", "", "800001", "100000", "0", "en",
                                   "space, adventure, robots");

    CreateTestZip(inpxPath, {
        {"fb2-kw.zip.inp", content},
        {"collection.info", "Keywords\nkw\n65536\n"},
        {"version.info", "20240101\r\n"}
    });

    auto u8path = inpxPath.u8string();
    CInpParser parser;
    auto books = parser.Parse(std::string(u8path.begin(), u8path.end()));

    REQUIRE(books.size() == 1);
    REQUIRE(books[0].keywords == "space, adventure, robots");
}

TEST_CASE("CInpParser: .inp file with truncated lines is parsed gracefully", "[inpx][edge]")
{
    CTempDir tempDir;
    std::filesystem::path inpxPath = tempDir.GetPath() / "truncated.inpx";

    // A severely truncated line — far fewer fields than expected
    std::string truncatedContent = std::string("Author,X,:\x04genre:\x04Title Only") + "\r\n";

    CreateTestZip(inpxPath, {
        {"fb2-trunc.zip.inp", truncatedContent},
        {"collection.info", "Truncated\ntr\n65536\n"},
        {"version.info", "20240101\r\n"}
    });

    auto u8path = inpxPath.u8string();
    CInpParser parser;
    // Should not throw — truncated lines are either skipped or partially parsed
    REQUIRE_NOTHROW(parser.Parse(std::string(u8path.begin(), u8path.end())));
}

TEST_CASE("CInpParser: completely empty .inp file produces no books", "[inpx][edge]")
{
    CTempDir tempDir;
    std::filesystem::path inpxPath = tempDir.GetPath() / "empty_inp.inpx";

    CreateTestZip(inpxPath, {
        {"fb2-empty.zip.inp", ""},  // empty .inp file
        {"collection.info", "Empty INP\nei\n65536\n"},
        {"version.info", "20240101\r\n"}
    });

    auto u8path = inpxPath.u8string();
    CInpParser parser;
    auto books = parser.Parse(std::string(u8path.begin(), u8path.end()));
    REQUIRE(books.empty());
}

TEST_CASE("CInpParser: line with series and series number is parsed", "[inpx][edge]")
{
    CTempDir tempDir;
    std::filesystem::path inpxPath = tempDir.GetPath() / "series.inpx";

    std::string content = MakeLine("Tolkien,J.R.R.,:", "fantasy:", "The Fellowship of the Ring",
                                   "The Lord of the Rings", "1", "900001");

    CreateTestZip(inpxPath, {
        {"fb2-ser.zip.inp", content},
        {"collection.info", "Series Test\nser\n65536\n"},
        {"version.info", "20240101\r\n"}
    });

    auto u8path = inpxPath.u8string();
    CInpParser parser;
    auto books = parser.Parse(std::string(u8path.begin(), u8path.end()));

    REQUIRE(books.size() == 1);
    REQUIRE(books[0].series == "The Lord of the Rings");
    REQUIRE(books[0].seriesNumber == 1);
}

TEST_CASE("CInpParser: invalid numeric fields keep default values", "[inpx][edge]")
{
    CTempDir tempDir;
    std::filesystem::path inpxPath = tempDir.GetPath() / "invalid_numbers.inpx";

    std::ostringstream ss;
    ss << "Numeric,Test,:" << SEP << "sf:" << SEP << "Broken Numbers" << SEP
       << "Series" << SEP << "oops" << SEP
       << "910001" << SEP << "badsize" << SEP << "910001" << SEP
       << "0" << SEP << "fb2" << SEP
       << "2023-06-01" << SEP << "ru" << SEP
       << "NaN" << SEP << "" << SEP << "" << "\r\n";

    CreateTestZip(inpxPath, {
        {"fb2-invalid-numbers.zip.inp", ss.str()},
        {"collection.info", "Invalid Numbers\ninum\n65536\n"},
        {"version.info", "20240101\r\n"}
    });

    auto u8path = inpxPath.u8string();
    CInpParser parser;

    REQUIRE_NOTHROW(parser.Parse(std::string(u8path.begin(), u8path.end())));
    auto books = parser.Parse(std::string(u8path.begin(), u8path.end()));

    REQUIRE(books.size() == 1);
    REQUIRE(books[0].seriesNumber == 0);
    REQUIRE(books[0].fileSize == 0);
    REQUIRE(books[0].rating == 0);
}

TEST_CASE("CInpParser: line with too many delimiters is handled without crash", "[inpx][edge]")
{
    CTempDir tempDir;
    std::filesystem::path inpxPath = tempDir.GetPath() / "too_many_sep.inpx";

    // A line with 100 separators (expected ~20)
    std::string malformed(100, SEP);
    malformed += "\r\n";

    CreateTestZip(inpxPath, {
        {"fb2-malformed.zip.inp", malformed},
        {"collection.info", "Malformed\nmal\n65536\n"},
        {"version.info", "20240101\r\n"}
    });

    auto u8path = inpxPath.u8string();
    CInpParser parser;
    // Parser should handle this without crash (likely skipping it as invalid)
    REQUIRE_NOTHROW(parser.Parse(std::string(u8path.begin(), u8path.end())));
}
