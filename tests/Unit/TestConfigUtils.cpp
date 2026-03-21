#include <catch2/catch_test_macros.hpp>

#include "Config/AppConfig.hpp"

#include <filesystem>
#include <string>

using namespace Librium::Config;

TEST_CASE("Config::Utf8ToPath", "[config]")
{
    SECTION("ASCII path round-trips correctly")
    {
        auto p = Utf8ToPath("hello/world.txt");
        REQUIRE(p.filename() == "world.txt");
        REQUIRE(p.parent_path().filename() == "hello");
    }

    SECTION("Empty string produces empty path")
    {
        auto p = Utf8ToPath("");
        REQUIRE(p.empty());
    }

    SECTION("Cyrillic UTF-8 path is preserved")
    {
        // "библиотека/книги.db" - valid UTF-8 Cyrillic
        std::string utf8 = "\xD0\xB1\xD0\xB8\xD0\xB1\xD0\xBB\xD0\xB8\xD0\xBE"
                           "\xD1\x82\xD0\xB5\xD0\xBA\xD0\xB0/"
                           "\xD0\xBA\xD0\xBD\xD0\xB8\xD0\xB3\xD0\xB8.db";
        auto p = Utf8ToPath(utf8);
        REQUIRE_FALSE(p.empty());
        // Round-trip via u8string must match the original bytes
        auto u8 = p.u8string();
        std::string roundtrip(u8.begin(), u8.end());
        REQUIRE(roundtrip == utf8);
    }

    SECTION("Path with spaces is handled correctly")
    {
        auto p = Utf8ToPath("my books/war and peace.fb2");
        REQUIRE(p.filename() == "war and peace.fb2");
    }

    SECTION("Dot and dotdot components are preserved")
    {
        auto p = Utf8ToPath("./relative/path.db");
        REQUIRE_FALSE(p.empty());
    }
}

TEST_CASE("Config::GetBookMetaDir", "[config]")
{
    SECTION("Builds path: <dbDir>/meta/<id>")
    {
        std::filesystem::path dbPath = "data/library.db";
        auto metaDir = GetBookMetaDir(dbPath, 42);
        REQUIRE(metaDir == std::filesystem::path("data/meta/42"));
    }

    SECTION("Id is used verbatim as directory name")
    {
        std::filesystem::path dbPath = "library.db";
        REQUIRE(GetBookMetaDir(dbPath, 1).filename()   == "1");
        REQUIRE(GetBookMetaDir(dbPath, 999).filename() == "999");
    }

    SECTION("Different ids produce different paths")
    {
        std::filesystem::path dbPath = "lib.db";
        auto dir1 = GetBookMetaDir(dbPath, 1);
        auto dir2 = GetBookMetaDir(dbPath, 2);
        REQUIRE(dir1 != dir2);
    }

    SECTION("Parent is always the 'meta' directory next to db")
    {
        std::filesystem::path dbPath = "some/deep/path/library.db";
        auto metaDir = GetBookMetaDir(dbPath, 7);
        REQUIRE(metaDir.parent_path().filename() == "meta");
        REQUIRE(metaDir.parent_path().parent_path() == dbPath.parent_path());
    }

    SECTION("Works with absolute path to database")
    {
        std::filesystem::path dbPath = std::filesystem::temp_directory_path() / "library.db";
        auto metaDir = GetBookMetaDir(dbPath, 100);
        REQUIRE(metaDir.filename() == "100");
        REQUIRE(metaDir.parent_path().filename() == "meta");
    }
}
