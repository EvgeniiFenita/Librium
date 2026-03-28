#include <catch2/catch_test_macros.hpp>
#include "Database/SearchQueryParser.hpp"

using namespace Librium::Db;

TEST_CASE("SearchQueryParser: basic parsing", "[db][query]")
{
    SECTION("Empty input")
    {
        auto token = ParseSearchQuery("");
        REQUIRE(token.mode == ESearchMode::Prefix);
        REQUIRE(token.value == "");
    }

    SECTION("Default prefix mode")
    {
        auto token = ParseSearchQuery("Pushkin");
        REQUIRE(token.mode == ESearchMode::Prefix);
        REQUIRE(token.value == "Pushkin");
    }

    SECTION("Exact match operator")
    {
        auto token = ParseSearchQuery("=Pushkin");
        REQUIRE(token.mode == ESearchMode::Exact);
        REQUIRE(token.value == "Pushkin");
    }

    SECTION("Contains operator")
    {
        auto token = ParseSearchQuery("*robot");
        REQUIRE(token.mode == ESearchMode::Contains);
        REQUIRE(token.value == "robot");
    }
}

TEST_CASE("SearchQueryParser: SQL building", "[db][query]")
{
    std::string sql, bind;

    SECTION("Prefix SQL")
    {
        SSearchToken token{ESearchMode::Prefix, "Push"};
        BuildSearchSql(token, "title", sql, bind);
        REQUIRE(sql == " AND title LIKE librium_upper(?) ESCAPE '\\' ");
        REQUIRE(bind == "Push%");
    }

    SECTION("Exact SQL")
    {
        SSearchToken token{ESearchMode::Exact, "Pushkin"};
        BuildSearchSql(token, "title", sql, bind);
        REQUIRE(sql == " AND title = librium_upper(?) ");
        REQUIRE(bind == "Pushkin");
    }

    SECTION("Contains SQL")
    {
        SSearchToken token{ESearchMode::Contains, "robot"};
        BuildSearchSql(token, "title", sql, bind);
        REQUIRE(sql == " AND title LIKE librium_upper(?) ESCAPE '\\' ");
        REQUIRE(bind == "%robot%");
    }
}
