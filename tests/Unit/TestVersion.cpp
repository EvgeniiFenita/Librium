#include <catch2/catch_test_macros.hpp>

#include "Version.hpp"

TEST_CASE("Librium version constants", "[version]")
{
    REQUIRE(Librium::Apps::VersionMajor == 0);
    REQUIRE(Librium::Apps::VersionMinor == 2);
    REQUIRE(std::string(Librium::Apps::VersionString) == "0.2.0");
}
