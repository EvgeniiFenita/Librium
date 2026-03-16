#include <catch2/catch_test_macros.hpp>

#include "Version.hpp"

TEST_CASE("Librium version constants", "[version]") 
{
    REQUIRE(Librium::Apps::Indexer::VersionMajor == 0);
    REQUIRE(Librium::Apps::Indexer::VersionMinor == 1);
    REQUIRE(std::string(Librium::Apps::Indexer::VersionString) == "0.1.0");
}






