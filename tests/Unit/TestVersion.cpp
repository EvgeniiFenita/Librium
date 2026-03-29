#include <catch2/catch_test_macros.hpp>

#include "Version.hpp"

TEST_CASE("Librium version constants", "[version]")
{
    REQUIRE(Librium::Apps::VersionMajor == 1);
    REQUIRE(Librium::Apps::VersionMinor == 0);
    REQUIRE(std::string(Librium::Apps::VersionString) == "1.0.0");
}
