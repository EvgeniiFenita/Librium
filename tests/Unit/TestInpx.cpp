#include <catch2/catch_test_macros.hpp>

#include "Inpx/InpParser.hpp"
#include <filesystem>

using namespace Librium::Inpx;

const std::string k_inpx = LIBRIUM_TEST_DATA_DIR "/test.inpx";

TEST_CASE("SAuthor::FullName full", "[inpx]")
{
    REQUIRE(SAuthor{"Толстой","Лев","Николаевич"}.FullName() == "Толстой, Лев Николаевич");
}

TEST_CASE("SAuthor::FullName no middle", "[inpx]")
{
    REQUIRE(SAuthor{"Пушкин","Александр",""}.FullName() == "Пушкин, Александр");
}

TEST_CASE("SAuthor::IsEmpty", "[inpx]")
{
    REQUIRE(SAuthor{}.IsEmpty());
    REQUIRE_FALSE(SAuthor{"X","",""}.IsEmpty());
}

TEST_CASE("InpParser streaming", "[inpx]")
{
    CInpParser parser;
    std::vector<SBookRecord> books;
    parser.ParseStreaming(k_inpx, [&](SBookRecord&& r)
    {
        books.push_back(std::move(r));
        return true;
    });

    REQUIRE(books.size() > 0);
    REQUIRE(parser.LastStats().parsedOk == books.size());
}

TEST_CASE("InpParser full parse", "[inpx]")
{
    CInpParser parser;
    auto books = parser.Parse(k_inpx);
    REQUIRE(books.size() > 0);
    REQUIRE(parser.LastStats().parsedOk == books.size());
}

TEST_CASE("InpParser fields verification", "[inpx]")
{
    CInpParser parser;
    auto books = parser.Parse(k_inpx);
    
    bool found = false;
    for (const auto& b : books)
    {
        if (b.libId == "100001")
        {
            REQUIRE(b.title == "Война и мир");
            REQUIRE(b.authors.size() == 1);
            REQUIRE(b.authors[0].lastName == "Толстой");
            REQUIRE(b.language == "ru");
            found = true;
            break;
        }
    }
    REQUIRE(found);
}
