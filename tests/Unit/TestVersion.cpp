#include <catch2/catch_test_macros.hpp>

#include "Version.hpp"

TEST_CASE("libindexer version constants", "[version]")
{
    REQUIRE(LibIndexer::Apps::Indexer::VersionMajor == 0);
    REQUIRE(LibIndexer::Apps::Indexer::VersionMinor == 1);
    REQUIRE(std::string(LibIndexer::Apps::Indexer::VersionString) == "0.1.0");
}
