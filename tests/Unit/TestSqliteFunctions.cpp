#include <catch2/catch_test_macros.hpp>
#include "Database/SqliteDatabase.hpp"
#include "Database/SqlStatement.hpp"
#include <memory>

using namespace Librium::Db;

TEST_CASE("SqliteFunctions: librium_upper", "[db][unicode]")
{
    // CSqliteDatabase automatically registers custom functions in constructor
    CSqliteDatabase db(":memory:", 1000, 0);

    auto testUpper = [&](const std::string& input) -> std::string {
        auto stmt = db.Prepare("SELECT librium_upper(?)");
        stmt->BindText(1, input);
        stmt->Step();
        return stmt->ColumnText(0);
    };

    SECTION("ASCII characters")
    {
        REQUIRE(testUpper("hello") == "HELLO");
        REQUIRE(testUpper("World123") == "WORLD123");
    }

    SECTION("Cyrillic characters")
    {
        REQUIRE(testUpper("пушкин") == "ПУШКИН");
        REQUIRE(testUpper("Кириллица") == "КИРИЛЛИЦА");
        REQUIRE(testUpper("ёжик") == "ЁЖИК");
    }

    SECTION("Ukrainian characters")
    {
        REQUIRE(testUpper("і") == "І");
        REQUIRE(testUpper("ї") == "Ї");
        REQUIRE(testUpper("є") == "Є");
        REQUIRE(testUpper("ґ") == "Ґ");
    }

    SECTION("Mixed and symbols")
    {
        REQUIRE(testUpper("Mixed Текст 123!") == "MIXED ТЕКСТ 123!");
    }
}
