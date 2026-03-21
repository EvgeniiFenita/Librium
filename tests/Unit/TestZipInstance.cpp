#include <catch2/catch_test_macros.hpp>

#include "Zip/ZipReader.hpp"
#include "TestUtils.hpp"

#include <filesystem>
#include <string>
#include <vector>

using namespace Librium::Zip;
using namespace Librium::Tests;

TEST_CASE("CZipReader instance constructor", "[zip]")
{
    CTempDir tempDir;
    std::filesystem::path zipPath = tempDir.GetPath() / "instance_ctor.zip";

    CreateTestZip(zipPath, {
        {"readme.txt", "some readme content"},
        {"data/file.bin", "\x00\x01\x02\x03"}
    });

    SECTION("Opens valid archive without throwing")
    {
        REQUIRE_NOTHROW(CZipReader{zipPath});
    }

    SECTION("Throws CZipError for missing archive")
    {
        REQUIRE_THROWS_AS(CZipReader{tempDir.GetPath() / "no_such.zip"}, CZipError);
    }
}

TEST_CASE("CZipReader::Path()", "[zip]")
{
    CTempDir tempDir;
    std::filesystem::path zipPath = tempDir.GetPath() / "path_test.zip";
    CreateTestZip(zipPath, {{"a.txt", "aaa"}});

    SECTION("Returns the path used to open the archive")
    {
        CZipReader reader(zipPath);
        REQUIRE(reader.Path() == zipPath);
    }
}

TEST_CASE("CZipReader instance ReadEntry()", "[zip]")
{
    CTempDir tempDir;
    std::filesystem::path zipPath = tempDir.GetPath() / "read_entry.zip";

    CreateTestZip(zipPath, {
        {"hello.txt",       "hello from instance"},
        {"subdir/deep.txt", "deep content"}
    });

    SECTION("Reads top-level entry")
    {
        CZipReader reader(zipPath);
        auto data = reader.ReadEntry("hello.txt");
        std::string content(data.begin(), data.end());
        REQUIRE(content == "hello from instance");
    }

    SECTION("Reads entry in subdirectory")
    {
        CZipReader reader(zipPath);
        auto data = reader.ReadEntry("subdir/deep.txt");
        std::string content(data.begin(), data.end());
        REQUIRE(content == "deep content");
    }

    SECTION("Throws CZipError for missing entry")
    {
        CZipReader reader(zipPath);
        REQUIRE_THROWS_AS(reader.ReadEntry("no_such_entry.txt"), CZipError);
    }

    SECTION("Can read multiple entries from same instance")
    {
        CZipReader reader(zipPath);
        auto d1 = reader.ReadEntry("hello.txt");
        auto d2 = reader.ReadEntry("subdir/deep.txt");
        REQUIRE(!d1.empty());
        REQUIRE(!d2.empty());
        REQUIRE(d1 != d2);
    }
}

TEST_CASE("CZipReader::IterateEntryNames", "[zip]")
{
    CTempDir tempDir;
    std::filesystem::path zipPath = tempDir.GetPath() / "iter_names.zip";

    CreateTestZip(zipPath, {
        {"alpha.txt", "aaa"},
        {"beta.txt",  "bbb"},
        {"gamma.txt", "ccc"}
    });

    SECTION("Visits all entries with correct metadata")
    {
        int count = 0;
        CZipReader::IterateEntryNames(zipPath, [&](const SZipEntry& entry)
        {
            REQUIRE_FALSE(entry.name.empty());
            REQUIRE(entry.uncompressedSize > 0);
            ++count;
            return true;
        });
        REQUIRE(count == 3);
    }

    SECTION("Stops iteration when callback returns false")
    {
        int count = 0;
        CZipReader::IterateEntryNames(zipPath, [&](const SZipEntry&)
        {
            ++count;
            return false; // stop after first
        });
        REQUIRE(count == 1);
    }

    SECTION("Collects all entry names")
    {
        std::vector<std::string> names;
        CZipReader::IterateEntryNames(zipPath, [&](const SZipEntry& entry)
        {
            names.push_back(entry.name);
            return true;
        });
        REQUIRE(names.size() == 3);
    }

    SECTION("No content read during name iteration (lightweight)")
    {
        // IterateEntryNames should NOT need to decompress data
        // Verify it completes without issue even on a large-ish entry
        CTempDir td2;
        std::filesystem::path bigZip = td2.GetPath() / "big.zip";
        std::string bigContent(10000, 'X');
        CreateTestZip(bigZip, {{"big.txt", bigContent}});

        int count = 0;
        CZipReader::IterateEntryNames(bigZip, [&](const SZipEntry& entry)
        {
            // Should have size info without decompressing
            REQUIRE(entry.uncompressedSize == 10000);
            ++count;
            return true;
        });
        REQUIRE(count == 1);
    }
}
