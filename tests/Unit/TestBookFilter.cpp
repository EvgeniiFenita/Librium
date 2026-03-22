#include <catch2/catch_test_macros.hpp>

#include "Config/AppConfig.hpp"
#include "Inpx/BookRecord.hpp"

using namespace Librium::Config;

namespace {

Librium::Inpx::SBookRecord MakeFilterRec(
    const std::string& lang     = "ru",
    uint64_t           size     = 100000,
    const std::string& genre    = "sf",
    const std::string& keywords = "")
{
    Librium::Inpx::SBookRecord r;
    r.language = lang;
    r.fileSize = size;
    r.genres   = {genre};
    r.keywords = keywords;
    r.authors.push_back({"Smith", "John", ""});
    return r;
}

Librium::Inpx::SBookRecord MakeRecWithAuthor(const std::string& lastName)
{
    Librium::Inpx::SBookRecord r;
    r.language = "ru";
    r.fileSize = 100000;
    r.genres   = {"sf"};
    r.authors.push_back({lastName, "First", ""});
    return r;
}

} // namespace

TEST_CASE("BookFilter genre filters", "[config]")
{
    SECTION("includeGenres allows only listed genres")
    {
        SFiltersConfig f;
        f.includeGenres = {"sf", "fantasy"};
        CBookFilter filter(f);
        REQUIRE(filter.ShouldInclude(MakeFilterRec("ru", 100000, "sf")));
        REQUIRE(filter.ShouldInclude(MakeFilterRec("ru", 100000, "fantasy")));
        REQUIRE_FALSE(filter.ShouldInclude(MakeFilterRec("ru", 100000, "detective")));
        REQUIRE_FALSE(filter.ShouldInclude(MakeFilterRec("ru", 100000, "romance")));
    }

    SECTION("excludeGenres blocks listed genres")
    {
        SFiltersConfig f;
        f.excludeGenres = {"romance", "erotica"};
        CBookFilter filter(f);
        REQUIRE(filter.ShouldInclude(MakeFilterRec("ru", 100000, "sf")));
        REQUIRE(filter.ShouldInclude(MakeFilterRec("ru", 100000, "fantasy")));
        REQUIRE_FALSE(filter.ShouldInclude(MakeFilterRec("ru", 100000, "romance")));
        REQUIRE_FALSE(filter.ShouldInclude(MakeFilterRec("ru", 100000, "erotica")));
    }

    SECTION("excludeGenres beats includeGenres when both list the same genre")
    {
        // Implementation: include-check runs first (passes), then exclude-check runs and rejects.
        SFiltersConfig f;
        f.includeGenres = {"sf"};
        f.excludeGenres = {"sf"};
        CBookFilter filter(f);
        // Book with "sf": passes include check, then excluded → rejected
        REQUIRE_FALSE(filter.ShouldInclude(MakeFilterRec("ru", 100000, "sf")));
        // Book with "fantasy": fails include check immediately → rejected
        REQUIRE_FALSE(filter.ShouldInclude(MakeFilterRec("ru", 100000, "fantasy")));
    }
}

TEST_CASE("BookFilter size filters", "[config]")
{
    SECTION("maxFileSize blocks oversized books")
    {
        SFiltersConfig f;
        f.maxFileSize = 200000;
        CBookFilter filter(f);
        REQUIRE(filter.ShouldInclude(MakeFilterRec("ru", 100000)));
        REQUIRE(filter.ShouldInclude(MakeFilterRec("ru", 200000)));
        REQUIRE_FALSE(filter.ShouldInclude(MakeFilterRec("ru", 200001)));
        REQUIRE_FALSE(filter.ShouldInclude(MakeFilterRec("ru", 1000000)));
    }

    SECTION("minFileSize and maxFileSize together form a range")
    {
        SFiltersConfig f;
        f.minFileSize = 50000;
        f.maxFileSize = 500000;
        CBookFilter filter(f);
        REQUIRE_FALSE(filter.ShouldInclude(MakeFilterRec("ru", 10000)));   // too small
        REQUIRE(filter.ShouldInclude(MakeFilterRec("ru", 100000)));        // in range
        REQUIRE(filter.ShouldInclude(MakeFilterRec("ru", 500000)));        // at boundary
        REQUIRE_FALSE(filter.ShouldInclude(MakeFilterRec("ru", 600000))); // too large
    }

    SECTION("Zero maxFileSize means no upper limit")
    {
        SFiltersConfig f;
        f.maxFileSize = 0; // disabled
        CBookFilter filter(f);
        REQUIRE(filter.ShouldInclude(MakeFilterRec("ru", 999999999)));
    }
}

TEST_CASE("BookFilter author exclusion", "[config]")
{
    SECTION("excludeAuthors blocks books by author using substring match on 'lastName firstName'")
    {
        SFiltersConfig f;
        f.excludeAuthors = {"Banned", "Blocked"};
        CBookFilter filter(f);
        REQUIRE(filter.ShouldInclude(MakeRecWithAuthor("Allowed")));
        REQUIRE_FALSE(filter.ShouldInclude(MakeRecWithAuthor("Banned")));
        REQUIRE_FALSE(filter.ShouldInclude(MakeRecWithAuthor("Blocked")));
    }

    SECTION("excludeAuthors partial name match also excludes")
    {
        SFiltersConfig f;
        f.excludeAuthors = {"Bad"};  // matches "Badly" because it's a substring
        CBookFilter filter(f);
        REQUIRE_FALSE(filter.ShouldInclude(MakeRecWithAuthor("Badly")));
        REQUIRE(filter.ShouldInclude(MakeRecWithAuthor("Good")));
    }

    SECTION("excludeAuthors with empty list includes all authors")
    {
        SFiltersConfig f;
        f.excludeAuthors = {};
        CBookFilter filter(f);
        REQUIRE(filter.ShouldInclude(MakeRecWithAuthor("Anyone")));
    }
}

TEST_CASE("BookFilter keyword exclusion", "[config]")
{
    SECTION("excludeKeywords filters books with matching keywords")
    {
        SFiltersConfig f;
        f.excludeKeywords = {"spam", "junk"};
        CBookFilter filter(f);
        REQUIRE_FALSE(filter.ShouldInclude(MakeFilterRec("ru", 100000, "sf", "spam")));
        REQUIRE_FALSE(filter.ShouldInclude(MakeFilterRec("ru", 100000, "sf", "junk")));
        REQUIRE(filter.ShouldInclude(MakeFilterRec("ru", 100000, "sf", "clean")));
    }

    SECTION("excludeKeywords with empty list passes all books")
    {
        SFiltersConfig f;
        f.excludeKeywords = {};
        CBookFilter filter(f);
        REQUIRE(filter.ShouldInclude(MakeFilterRec("ru", 100000, "sf", "anything goes")));
    }

    SECTION("excludeKeywords partial match also excludes")
    {
        SFiltersConfig f;
        f.excludeKeywords = {"spam"};
        CBookFilter filter(f);
        REQUIRE_FALSE(filter.ShouldInclude(MakeFilterRec("ru", 100000, "sf", "contains spam text")));
        REQUIRE(filter.ShouldInclude(MakeFilterRec("ru", 100000, "sf", "clean text")));
    }

    SECTION("book with empty keywords field is never excluded by keyword filter")
    {
        SFiltersConfig f;
        f.excludeKeywords = {"spam"};
        CBookFilter filter(f);
        REQUIRE(filter.ShouldInclude(MakeFilterRec("ru", 100000, "sf", "")));
    }

    SECTION("excludeKeywords exclusion reason is non-empty")
    {
        SFiltersConfig f;
        f.excludeKeywords = {"spam"};
        CBookFilter filter(f);
        auto result = filter.ShouldInclude(MakeFilterRec("ru", 100000, "sf", "spam"));
        REQUIRE_FALSE(result.included);
        REQUIRE_FALSE(result.reason.empty());
    }
}

TEST_CASE("BookFilter combined filters", "[config]")
{
    SECTION("Language + genre combined filter")
    {
        SFiltersConfig f;
        f.includeLanguages = {"en"};
        f.includeGenres    = {"sf"};
        CBookFilter filter(f);
        REQUIRE(filter.ShouldInclude(MakeFilterRec("en", 100000, "sf")));
        REQUIRE_FALSE(filter.ShouldInclude(MakeFilterRec("ru", 100000, "sf")));  // wrong lang
        REQUIRE_FALSE(filter.ShouldInclude(MakeFilterRec("en", 100000, "romance"))); // wrong genre
    }

    SECTION("Language + size combined filter")
    {
        SFiltersConfig f;
        f.includeLanguages = {"ru"};
        f.minFileSize      = 50000;
        CBookFilter filter(f);
        REQUIRE(filter.ShouldInclude(MakeFilterRec("ru", 100000)));
        REQUIRE_FALSE(filter.ShouldInclude(MakeFilterRec("en", 100000))); // wrong lang
        REQUIRE_FALSE(filter.ShouldInclude(MakeFilterRec("ru", 1000)));   // too small
    }

    SECTION("SFilterResult reason is non-empty on exclusion")
    {
        SFiltersConfig f;
        f.excludeLanguages = {"en"};
        CBookFilter filter(f);
        auto result = filter.ShouldInclude(MakeFilterRec("en"));
        REQUIRE_FALSE(result.included);
        REQUIRE_FALSE(result.reason.empty());
    }
}
