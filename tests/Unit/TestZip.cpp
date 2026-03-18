#include <catch2/catch_test_macros.hpp>
#include <string>
#include <filesystem>
#include "Zip/ZipReader.hpp"
#include "TestUtils.hpp"

using namespace Librium::Zip;
using namespace Librium::Tests;

TEST_CASE("CZipReader operations", "[zip]")
{
    CTempDir tempDir;
    std::filesystem::path zipPath = tempDir.GetPath() / "test.zip";
    
    CreateTestZip(zipPath, {
        {"hello.txt", "hello world from test zip"},
        {"subdir/another.txt", "another file"}
    });

    SECTION("throws for missing archive")
    {
        REQUIRE_THROWS_AS(CZipReader::ListEntries(std::filesystem::path("/no/such.zip")), CZipError);
    }

    SECTION("ListEntries finds hello.txt")
    {
        const auto entries = CZipReader::ListEntries(zipPath);
        bool found = false;
        for (const auto& e : entries)
        {
            if (e.name == "hello.txt")
            {
                found = true;
                REQUIRE(e.uncompressedSize > 0);
            }
        }
        REQUIRE(found);
    }

    SECTION("ReadEntry returns correct content")
    {
        const auto data = CZipReader::ReadEntry(zipPath, "hello.txt");
        REQUIRE(std::string(data.begin(), data.end()).find("hello") != std::string::npos);
    }

    SECTION("ReadEntry throws for missing entry")
    {
        REQUIRE_THROWS_AS(CZipReader::ReadEntry(zipPath, "no_such.txt"), CZipError);
    }

    SECTION("IterateEntries visits files")
    {
        int count = 0;
        CZipReader::IterateEntries(zipPath, [&](const std::string&, std::vector<uint8_t>)
        {
            ++count;
            return true;
        });
        REQUIRE(count >= 1);
    }

    SECTION("IterateEntries stops on false")
    {
        int count = 0;
        CZipReader::IterateEntries(zipPath, [&](const std::string&, std::vector<uint8_t>)
        {
            ++count;
            return false;
        });
        REQUIRE(count == 1);
    }
}
