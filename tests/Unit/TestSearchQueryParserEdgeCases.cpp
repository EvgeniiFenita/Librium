#include <catch2/catch_test_macros.hpp>
#include "Database/SearchQueryParser.hpp"

using namespace Librium::Db;

// NOTE: Basic parsing (empty, prefix, exact, contains) and SQL generation are
// already covered in TestSearchQueryParser.cpp. This file tests edge cases.

TEST_CASE("SearchQueryParser: operator-only input", "[db][query][edge]")
{
    SECTION("Exact operator with no value: '=' → empty exact value")
    {
        auto token = ParseSearchQuery("=");
        REQUIRE(token.mode == ESearchMode::Exact);
        REQUIRE(token.value == "");
    }

    SECTION("Contains operator with no value: '*' → empty contains value")
    {
        auto token = ParseSearchQuery("*");
        REQUIRE(token.mode == ESearchMode::Contains);
        REQUIRE(token.value == "");
    }
}

TEST_CASE("SearchQueryParser: whitespace input", "[db][query][edge]")
{
    SECTION("Whitespace-only string is treated as prefix with whitespace value")
    {
        auto token = ParseSearchQuery("   ");
        REQUIRE(token.mode == ESearchMode::Prefix);
        // The value may be trimmed or kept as-is — both are acceptable;
        // the key requirement is that it does not crash.
    }

    SECTION("Single space is handled without crash")
    {
        REQUIRE_NOTHROW(ParseSearchQuery(" "));
    }
}

TEST_CASE("SearchQueryParser: SQL LIKE wildcards in user input", "[db][query][edge]")
{
    // When the user literally types '%' or '_', those characters become part of the
    // bind value. In a PREPARED STATEMENT the bind value is not subject to SQL injection,
    // but for LIKE they act as wildcards unless the SQL uses ESCAPE.
    // These tests document current behaviour — not assert correctness.

    SECTION("Percent sign in prefix mode: bind value ends in '%%' (two wildcards)")
    {
        SSearchToken token{ESearchMode::Prefix, "50%"};
        std::string sql, bind;
        BuildSearchSql(token, "col", sql, bind);
        // Prefix appends '%': bind becomes "50%%"
        REQUIRE(bind == "50%%");
    }

    SECTION("Underscore in exact mode: passed through as literal underscore")
    {
        SSearchToken token{ESearchMode::Exact, "book_1"};
        std::string sql, bind;
        BuildSearchSql(token, "col", sql, bind);
        REQUIRE(bind == "book_1");
        REQUIRE(sql.find("= librium_upper(?)") != std::string::npos);
    }

    SECTION("Apostrophe in value does not corrupt SQL (prepared statement safety)")
    {
        // Apostrophes in the bind value are safe in prepared statements.
        SSearchToken token{ESearchMode::Prefix, "O'Brien"};
        std::string sql, bind;
        REQUIRE_NOTHROW(BuildSearchSql(token, "search_name", sql, bind));
        REQUIRE(bind == "O'Brien%");
    }
}

TEST_CASE("SearchQueryParser: very long input", "[db][query][edge]")
{
    SECTION("1000-character prefix query does not crash")
    {
        std::string longInput(1000, 'A');
        REQUIRE_NOTHROW(ParseSearchQuery(longInput));
        auto token = ParseSearchQuery(longInput);
        REQUIRE(token.mode == ESearchMode::Prefix);
        REQUIRE(token.value.size() == 1000);
    }

    SECTION("BuildSearchSql with long value does not crash")
    {
        SSearchToken token{ESearchMode::Contains, std::string(500, 'B')};
        std::string sql, bind;
        REQUIRE_NOTHROW(BuildSearchSql(token, "col", sql, bind));
        // Contains wraps with '%': "%BBB...BBB%"
        REQUIRE(bind.size() == 502);
        REQUIRE(bind.front() == '%');
        REQUIRE(bind.back() == '%');
    }
}

TEST_CASE("SearchQueryParser: mixed Cyrillic and Latin input", "[db][query][edge]")
{
    SECTION("Mixed Cyrillic + Latin prefix")
    {
        // "Пушkin" (starts Cyrillic, ends Latin)
        std::string mixed = "\xD0\x9F\xD1\x83\xD1\x88kin";
        auto token = ParseSearchQuery(mixed);
        REQUIRE(token.mode == ESearchMode::Prefix);
        REQUIRE(token.value == mixed);

        std::string sql, bind;
        BuildSearchSql(token, "search_name", sql, bind);
        REQUIRE(bind == mixed + "%");
    }

    SECTION("Contains mode with Cyrillic value")
    {
        // "*робот" → Contains mode, value "робот"
        std::string input = "*\xD1\x80\xD0\xBE\xD0\xB1\xD0\xBE\xD1\x82"; // *робот
        auto token = ParseSearchQuery(input);
        REQUIRE(token.mode == ESearchMode::Contains);

        std::string sql, bind;
        BuildSearchSql(token, "search_title", sql, bind);
        REQUIRE(bind.front() == '%');
        REQUIRE(bind.back() == '%');
    }
}
