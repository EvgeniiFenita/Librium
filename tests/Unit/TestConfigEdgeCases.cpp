#include <catch2/catch_test_macros.hpp>

#include "Config/AppConfig.hpp"
#include "Inpx/BookRecord.hpp"
#include "TestUtils.hpp"

#include <filesystem>
#include <fstream>

using namespace Librium::Config;
using namespace Librium::Inpx;
using namespace Librium::Tests;

// NOTE: Basic BookFilter (language exclude/include, minFileSize) is in TestConfig.cpp.
// This file tests additional filter fields and edge cases.

namespace {

SBookRecord MakeRec(const std::string& lang = "ru",
                    uint64_t           size  = 100000,
                    const std::string& genre = "sf")
{
    SBookRecord r;
    r.language = lang;
    r.fileSize = size;
    r.genres   = {genre};
    return r;
}

SBookRecord MakeRecWithAuthor(const std::string& lastName,
                               const std::string& firstName = "")
{
    SBookRecord r;
    r.language = "ru";
    r.fileSize = 100000;
    r.genres   = {"sf"};
    r.authors.push_back({lastName, firstName, ""});
    return r;
}

SBookRecord MakeRecWithKeywords(const std::string& keywords)
{
    SBookRecord r;
    r.language = "ru";
    r.fileSize = 100000;
    r.keywords = keywords;
    return r;
}

} // namespace

TEST_CASE("BookFilter: maxFileSize", "[config][filter]")
{
    SECTION("File exactly at max is included")
    {
        SFiltersConfig f;
        f.maxFileSize = 200000;
        CBookFilter filter(f);
        REQUIRE(filter.ShouldInclude(MakeRec("ru", 200000)));
    }

    SECTION("File above max is excluded")
    {
        SFiltersConfig f;
        f.maxFileSize = 200000;
        CBookFilter filter(f);
        REQUIRE_FALSE(filter.ShouldInclude(MakeRec("ru", 200001)));
    }

    SECTION("Zero maxFileSize means no upper limit")
    {
        SFiltersConfig f;
        f.maxFileSize = 0; // disabled
        CBookFilter filter(f);
        REQUIRE(filter.ShouldInclude(MakeRec("ru", 999999999ULL)));
    }
}

TEST_CASE("BookFilter: minFileSize boundary", "[config][filter]")
{
    SECTION("File exactly at min is included")
    {
        SFiltersConfig f;
        f.minFileSize = 50000;
        CBookFilter filter(f);
        REQUIRE(filter.ShouldInclude(MakeRec("ru", 50000)));
    }

    SECTION("File one byte below min is excluded")
    {
        SFiltersConfig f;
        f.minFileSize = 50000;
        CBookFilter filter(f);
        REQUIRE_FALSE(filter.ShouldInclude(MakeRec("ru", 49999)));
    }

    SECTION("Zero-size file is excluded by any non-zero minFileSize")
    {
        SFiltersConfig f;
        f.minFileSize = 1;
        CBookFilter filter(f);
        REQUIRE_FALSE(filter.ShouldInclude(MakeRec("ru", 0)));
    }
}

TEST_CASE("BookFilter: min and max combined", "[config][filter]")
{
    SFiltersConfig f;
    f.minFileSize = 50000;
    f.maxFileSize = 300000;
    CBookFilter filter(f);

    SECTION("File in range is included")
    {
        REQUIRE(filter.ShouldInclude(MakeRec("ru", 100000)));
    }

    SECTION("File below range is excluded")
    {
        REQUIRE_FALSE(filter.ShouldInclude(MakeRec("ru", 49999)));
    }

    SECTION("File above range is excluded")
    {
        REQUIRE_FALSE(filter.ShouldInclude(MakeRec("ru", 300001)));
    }
}

TEST_CASE("BookFilter: excludeGenres", "[config][filter]")
{
    SFiltersConfig f;
    f.excludeGenres = {"thriller"};
    CBookFilter filter(f);

    SECTION("Book with excluded genre is excluded")
    {
        REQUIRE_FALSE(filter.ShouldInclude(MakeRec("ru", 100000, "thriller")));
    }

    SECTION("Book with allowed genre is included")
    {
        REQUIRE(filter.ShouldInclude(MakeRec("ru", 100000, "sf")));
    }
}

TEST_CASE("BookFilter: includeGenres", "[config][filter]")
{
    SFiltersConfig f;
    f.includeGenres = {"sf"};
    CBookFilter filter(f);

    SECTION("Book with included genre is included")
    {
        REQUIRE(filter.ShouldInclude(MakeRec("ru", 100000, "sf")));
    }

    SECTION("Book with non-whitelisted genre is excluded")
    {
        REQUIRE_FALSE(filter.ShouldInclude(MakeRec("ru", 100000, "romance")));
    }
}

TEST_CASE("BookFilter: multiple includeLanguages", "[config][filter]")
{
    SFiltersConfig f;
    f.includeLanguages = {"ru", "en"};
    CBookFilter filter(f);

    SECTION("Russian book is included")
    {
        REQUIRE(filter.ShouldInclude(MakeRec("ru")));
    }

    SECTION("English book is included")
    {
        REQUIRE(filter.ShouldInclude(MakeRec("en")));
    }

    SECTION("Ukrainian book is excluded")
    {
        REQUIRE_FALSE(filter.ShouldInclude(MakeRec("uk")));
    }
}

TEST_CASE("BookFilter: excludeAuthors", "[config][filter]")
{
    SFiltersConfig f;
    f.excludeAuthors = {"Banned"};
    CBookFilter filter(f);

    SECTION("Book by excluded author is excluded")
    {
        REQUIRE_FALSE(filter.ShouldInclude(MakeRecWithAuthor("Banned", "Writer")));
    }

    SECTION("Book by allowed author is included")
    {
        REQUIRE(filter.ShouldInclude(MakeRecWithAuthor("Allowed", "Writer")));
    }

    SECTION("Book with no authors is included")
    {
        SBookRecord r;
        r.language = "ru";
        r.fileSize = 100000;
        REQUIRE(filter.ShouldInclude(r));
    }
}

TEST_CASE("BookFilter: excludeKeywords", "[config][filter]")
{
    SFiltersConfig f;
    f.excludeKeywords = {"spam"};
    CBookFilter filter(f);

    SECTION("Book with excluded keyword is excluded")
    {
        REQUIRE_FALSE(filter.ShouldInclude(MakeRecWithKeywords("spam, test")));
    }

    SECTION("Book with other keywords is included")
    {
        REQUIRE(filter.ShouldInclude(MakeRecWithKeywords("sci-fi, adventure")));
    }

    SECTION("Book with empty keywords is included")
    {
        REQUIRE(filter.ShouldInclude(MakeRecWithKeywords("")));
    }
}

TEST_CASE("AppConfig::Load from non-existent path throws", "[config]")
{
    REQUIRE_THROWS_AS(SAppConfig::Load("/no/such/path/config.json"), CConfigError);
}

TEST_CASE("AppConfig::Load from empty JSON uses defaults", "[config]")
{
    CTempDir tempDir;
    auto cfgPath = tempDir.GetPath() / "empty.json";
    { std::ofstream(cfgPath) << "{}"; }

    // An empty JSON object should not throw — missing fields get their defaults.
    SAppConfig cfg;
    auto u8 = cfgPath.u8string();
    REQUIRE_NOTHROW(cfg = SAppConfig::Load(std::string(u8.begin(), u8.end())));
    REQUIRE_FALSE(cfg.database.path.empty());
    REQUIRE(cfg.import.threadCount > 0);
}
