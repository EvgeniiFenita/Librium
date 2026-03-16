#include <catch2/catch_test_macros.hpp>
#include <fstream>
#include <string>
#include "Inpx/InpParser.hpp"

using namespace Librium::Inpx;

static const std::string k_inpx =
    std::string(LIBRIUM_TEST_DATA_DIR) + "/test_library.inpx";

TEST_CASE("CAuthor::FullName full", "[inpx]") 
{
    REQUIRE(CAuthor{"Толстой","Лев","Николаевич"}.FullName() == "Толстой, Лев Николаевич");
}
TEST_CASE("CAuthor::FullName no middle", "[inpx]") 
{
    REQUIRE(CAuthor{"Пушкин","Александр",""}.FullName() == "Пушкин, Александр");
}
TEST_CASE("CAuthor::IsEmpty", "[inpx]") 
{
    REQUIRE(CAuthor{}.IsEmpty());
    REQUIRE_FALSE(CAuthor{"X","",""}.IsEmpty());
}

TEST_CASE("ParseStreaming: 3 valid books, 1 deleted", "[inpx]") 
{
    std::ifstream f(k_inpx);
    if (!f.good()) 
{ WARN("Skipping"); return; }

    CInpParser parser;
    std::vector<CBookRecord> books;
    parser.ParseStreaming(k_inpx, [&](CBookRecord&& r) 
{ books.push_back(std::move(r)); return true; });

    REQUIRE(books.size() == 3);
    REQUIRE(parser.LastStats().skippedDeleted == 1);
}

TEST_CASE("No empty authors or genres from trailing colon", "[inpx]") 
{
    std::ifstream f(k_inpx);
    if (!f.good()) 
{ WARN("Skipping"); return; }

    CInpParser parser;
    std::vector<CBookRecord> books;
    parser.ParseStreaming(k_inpx, [&](CBookRecord&& r) 
{ books.push_back(std::move(r)); return true; });

    for (const auto& b : books) 
{
        for (const auto& a : b.authors) REQUIRE_FALSE(a.IsEmpty());
        for (const auto& g : b.genres)  REQUIRE_FALSE(g.empty());
    }
}

TEST_CASE("archiveName has no .inp suffix", "[inpx]") 
{
    std::ifstream f(k_inpx);
    if (!f.good()) 
{ WARN("Skipping"); return; }

    CInpParser parser;
    std::vector<CBookRecord> books;
    parser.ParseStreaming(k_inpx, [&](CBookRecord&& r) 
{ books.push_back(std::move(r)); return true; });

    for (const auto& b : books)
        REQUIRE(b.archiveName.find(".inp") == std::string::npos);
}






