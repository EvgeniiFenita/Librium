#include <catch2/catch_test_macros.hpp>

#include "Config/AppConfig.hpp"
#include "Inpx/BookRecord.hpp"
#include <filesystem>

using namespace Librium::Config;

namespace {

Librium::Inpx::SBookRecord MakeRec(const std::string& lang = "ru", uint64_t size = 100000)
{
    Librium::Inpx::SBookRecord r;
    r.language = lang;
    r.fileSize = size;
    return r;
}

} // namespace

TEST_CASE("AppConfig defaults", "[config]")
{
    auto c = SAppConfig::Defaults();
    REQUIRE(c.database.path == "library.db");
    REQUIRE(c.import.threadCount > 0);
}

TEST_CASE("AppConfig save/load", "[config]")
{
    std::string path = "test_config.json";
    auto c1 = SAppConfig::Defaults();
    c1.database.path = "custom.db";
    c1.Save(path);

    auto c2 = SAppConfig::Load(path);
    REQUIRE(c2.database.path == "custom.db");

    std::filesystem::remove(path);
}

TEST_CASE("BookFilter logic", "[config]")
{
    SECTION("Empty filter includes all")
    {
        REQUIRE(CBookFilter(SFiltersConfig{}).ShouldInclude(MakeRec()));
    }

    SECTION("Exclude language")
    {
        SFiltersConfig f;
        f.excludeLanguages = {"en"};
        CBookFilter filter(f);
        REQUIRE(filter.ShouldInclude(MakeRec("ru")));
        REQUIRE_FALSE(filter.ShouldInclude(MakeRec("en")));
    }

    SECTION("Include language")
    {
        SFiltersConfig f;
        f.includeLanguages = {"ru"};
        CBookFilter filter(f);
        REQUIRE(filter.ShouldInclude(MakeRec("ru")));
        REQUIRE_FALSE(filter.ShouldInclude(MakeRec("en")));
    }

    SECTION("Min file size")
    {
        SFiltersConfig f;
        f.minFileSize = 50000;
        CBookFilter filter(f);
        REQUIRE(filter.ShouldInclude(MakeRec("ru", 60000)));
        REQUIRE_FALSE(filter.ShouldInclude(MakeRec("ru", 40000)));
    }
}
