#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>
#include "Config/AppConfig.hpp"
#include "Inpx/BookRecord.hpp"

using namespace Librium;
using namespace Librium::Config;

namespace {
Inpx::CBookRecord MakeRec(const std::string& lang="ru",
                           const std::string& genre="prose",
                           uint64_t size=100000) 
{
    Inpx::CBookRecord r;
    r.language = lang; r.fileSize = size;
    r.genres.push_back(genre);
    return r;
}
} // namespace

TEST_CASE("Defaults valid", "[config]") 
{
    REQUIRE(CAppConfig::Defaults().import.threadCount > 0);
}
TEST_CASE("Load throws for missing file", "[config]") 
{
    REQUIRE_THROWS_AS(CAppConfig::Load("/no/such.json"), ConfigError);
}
TEST_CASE("Save and Load roundtrip", "[config]") 
{
    const std::string tmp = "test_cfg_rt.json";
    auto c = CAppConfig::Defaults();
    c.database.path = "my.db";
    c.filters.excludeLanguages = {"en","de"};
    c.Save(tmp);
    auto loaded = CAppConfig::Load(tmp);
    REQUIRE(loaded.database.path == "my.db");
    REQUIRE(loaded.filters.excludeLanguages.size() == 2);
    std::filesystem::remove(tmp);
}
TEST_CASE("CBookFilter no filters allows all", "[config]") 
{
    REQUIRE(CBookFilter(CFiltersConfig{}).ShouldInclude(MakeRec()));
}
TEST_CASE("CBookFilter excludeLanguages", "[config]") 
{
    CFiltersConfig f; f.excludeLanguages = {"en"};
    CBookFilter bf(f);
    REQUIRE_FALSE(bf.ShouldInclude(MakeRec("en")));
    REQUIRE(bf.ShouldInclude(MakeRec("ru")));
}
TEST_CASE("CBookFilter includeLanguages whitelist", "[config]") 
{
    CFiltersConfig f; f.includeLanguages = {"ru"};
    CBookFilter bf(f);
    REQUIRE(bf.ShouldInclude(MakeRec("ru")));
    REQUIRE_FALSE(bf.ShouldInclude(MakeRec("en")));
}
TEST_CASE("CBookFilter minFileSize", "[config]") 
{
    CFiltersConfig f; f.minFileSize = 50000;
    CBookFilter bf(f);
    REQUIRE_FALSE(bf.ShouldInclude(MakeRec("ru","prose",1000)));
    REQUIRE(bf.ShouldInclude(MakeRec("ru","prose",100000)));
}






