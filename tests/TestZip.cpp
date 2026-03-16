#include <catch2/catch_test_macros.hpp>
#include <string>
#include "Zip/ZipReader.hpp"

using namespace LibIndexer::Zip;

static const std::string k_zip = std::string(LIBINDEXER_TEST_DATA_DIR) + "/test.zip";

TEST_CASE("CZipReader throws for missing archive", "[zip]")
{
    REQUIRE_THROWS_AS(CZipReader::ListEntries("/no/such.zip"), CZipError);
}

TEST_CASE("CZipReader::ListEntries finds hello.txt", "[zip]")
{
    const auto entries = CZipReader::ListEntries(k_zip);
    bool found = false;
    for (const auto& e : entries)
        if (e.name == "hello.txt") { found = true; REQUIRE(e.uncompressedSize > 0); }
    REQUIRE(found);
}

TEST_CASE("CZipReader::ReadEntry returns correct content", "[zip]")
{
    const auto data = CZipReader::ReadEntry(k_zip, "hello.txt");
    REQUIRE(std::string(data.begin(), data.end()).find("hello") != std::string::npos);
}

TEST_CASE("CZipReader::ReadEntry throws for missing entry", "[zip]")
{
    REQUIRE_THROWS_AS(CZipReader::ReadEntry(k_zip, "no_such.txt"), CZipError);
}

TEST_CASE("CZipReader::IterateEntries visits files", "[zip]")
{
    int count = 0;
    CZipReader::IterateEntries(k_zip,
        [&](const std::string&, std::vector<uint8_t>) { ++count; return true; });
    REQUIRE(count >= 1);
}

TEST_CASE("CZipReader::IterateEntries stops on false", "[zip]")
{
    int count = 0;
    CZipReader::IterateEntries(k_zip,
        [&](const std::string&, std::vector<uint8_t>) { ++count; return false; });
    REQUIRE(count == 1);
}
