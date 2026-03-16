#include <catch2/catch_test_macros.hpp>
#include <fstream>
#include <string>
#include "Inpx/InpParser.hpp"

using namespace LibIndexer::Inpx;

static const std::string k_inpx =
    std::string(LIBINDEXER_TEST_DATA_DIR) + "/test_library.inpx";

TEST_CASE("Author::FullName full", "[inpx]")
{
    REQUIRE(Author{"Толстой","Лев","Николаевич"}.FullName() == "Толстой, Лев Николаевич");
}
TEST_CASE("Author::FullName no middle", "[inpx]")
{
    REQUIRE(Author{"Пушкин","Александр",""}.FullName() == "Пушкин, Александр");
}
TEST_CASE("Author::IsEmpty", "[inpx]")
{
    REQUIRE(Author{}.IsEmpty());
    REQUIRE_FALSE(Author{"X","",""}.IsEmpty());
}

TEST_CASE("ParseStreaming: 3 valid books, 1 deleted", "[inpx]")
{
    std::ifstream f(k_inpx);
    if (!f.good()) { WARN("Skipping"); return; }

    CInpParser parser;
    std::vector<BookRecord> books;
    parser.ParseStreaming(k_inpx, [&](BookRecord&& r){ books.push_back(std::move(r)); return true; });

    REQUIRE(books.size() == 3);
    REQUIRE(parser.LastStats().skippedDeleted == 1);
}

TEST_CASE("No empty authors or genres from trailing colon", "[inpx]")
{
    std::ifstream f(k_inpx);
    if (!f.good()) { WARN("Skipping"); return; }

    CInpParser parser;
    std::vector<BookRecord> books;
    parser.ParseStreaming(k_inpx, [&](BookRecord&& r){ books.push_back(std::move(r)); return true; });

    for (const auto& b : books)
    {
        for (const auto& a : b.authors) REQUIRE_FALSE(a.IsEmpty());
        for (const auto& g : b.genres)  REQUIRE_FALSE(g.empty());
    }
}

TEST_CASE("archiveName has no .inp suffix", "[inpx]")
{
    std::ifstream f(k_inpx);
    if (!f.good()) { WARN("Skipping"); return; }

    CInpParser parser;
    std::vector<BookRecord> books;
    parser.ParseStreaming(k_inpx, [&](BookRecord&& r){ books.push_back(std::move(r)); return true; });

    for (const auto& b : books)
        REQUIRE(b.archiveName.find(".inp") == std::string::npos);
}
