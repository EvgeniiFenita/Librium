#include <catch2/catch_test_macros.hpp>

#include "Zip/ZipReader.hpp"
#include "TestUtils.hpp"

#include <filesystem>
#include <fstream>
#include <map>
#include <string>

using namespace Librium::Zip;
using namespace Librium::Tests;

// NOTE: Basic ZipReader operations (list, read, iterate, missing archive/entry)
// are already tested in TestZip.cpp. This file tests additional edge cases.

TEST_CASE("CZipReader: ZIP containing Unicode (Cyrillic) filenames", "[zip][edge]")
{
    CTempDir tempDir;
    std::filesystem::path zipPath = tempDir.GetPath() / "unicode_names.zip";

    // "книга.fb2" — Cyrillic filename
    std::string cyrillicName = "\xD0\xBA\xD0\xBD\xD0\xB8\xD0\xB3\xD0\xB0.fb2";
    CreateTestZip(zipPath, {
        {cyrillicName, "FB2 content for Cyrillic filename"}
    });

    SECTION("ListEntries returns the Cyrillic filename")
    {
        auto entries = CZipReader::ListEntries(zipPath);
        REQUIRE(entries.size() == 1);
        REQUIRE(entries[0].name == cyrillicName);
    }

    SECTION("ReadEntry with Cyrillic name returns correct content")
    {
        auto data = CZipReader::ReadEntry(zipPath, cyrillicName);
        std::string content(data.begin(), data.end());
        REQUIRE(content.find("FB2 content") != std::string::npos);
    }
}

TEST_CASE("CZipReader: empty ZIP archive yields no entries", "[zip][edge]")
{
    CTempDir tempDir;
    std::filesystem::path zipPath = tempDir.GetPath() / "empty.zip";

    // A valid empty ZIP is a 22-byte "end-of-central-directory" record with
    // zero entries. libzip's ZIP_CREATE|ZIP_TRUNCATE does not flush the file
    // if no entries are added, so we write the minimal bytes ourselves.
    {
        // PK\x05\x06 + 18 zero bytes = 22-byte empty central directory
        static const uint8_t kEmptyZip[22] = {
            0x50, 0x4B, 0x05, 0x06,  // end-of-central-dir signature
            0x00, 0x00,              // disk number
            0x00, 0x00,              // start disk
            0x00, 0x00,              // entries on this disk
            0x00, 0x00,              // total entries
            0x00, 0x00, 0x00, 0x00,  // central directory size
            0x00, 0x00, 0x00, 0x00,  // central directory offset
            0x00, 0x00               // comment length
        };
        std::ofstream f(zipPath, std::ios::binary);
        f.write(reinterpret_cast<const char*>(kEmptyZip), sizeof(kEmptyZip));
    }

    SECTION("ListEntries on empty ZIP returns empty vector")
    {
        auto entries = CZipReader::ListEntries(zipPath);
        REQUIRE(entries.empty());
    }

    SECTION("IterateEntries on empty ZIP calls callback zero times")
    {
        int callCount = 0;
        CZipReader::IterateEntries(zipPath, [&](const std::string&, std::vector<uint8_t>)
        {
            ++callCount;
            return true;
        });
        REQUIRE(callCount == 0);
    }
}

TEST_CASE("CZipReader: ZIP with multiple files — all entries are enumerable", "[zip][edge]")
{
    CTempDir tempDir;
    std::filesystem::path zipPath = tempDir.GetPath() / "multi.zip";

    CreateTestZip(zipPath, {
        {"a.txt", "content a"},
        {"b.txt", "content b"},
        {"c.txt", "content c"}
    });

    SECTION("ListEntries returns all three files")
    {
        auto entries = CZipReader::ListEntries(zipPath);
        REQUIRE(entries.size() == 3);
    }

    SECTION("IterateEntries visits all three files and returns correct content")
    {
        std::map<std::string, std::string> seen;
        CZipReader::IterateEntries(zipPath, [&](const std::string& name, std::vector<uint8_t> data)
        {
            seen[name] = std::string(data.begin(), data.end());
            return true;
        });
        REQUIRE(seen.size() == 3);
        REQUIRE(seen["a.txt"] == "content a");
        REQUIRE(seen["b.txt"] == "content b");
        REQUIRE(seen["c.txt"] == "content c");
    }
}

TEST_CASE("CZipReader: ReadEntry throws CZipError for non-existent entry in existing archive", "[zip][edge]")
{
    CTempDir tempDir;
    std::filesystem::path zipPath = tempDir.GetPath() / "existing.zip";
    CreateTestZip(zipPath, {{"real.txt", "content"}});

    REQUIRE_THROWS_AS(CZipReader::ReadEntry(zipPath, "phantom.txt"), CZipError);
}

TEST_CASE("CZipReader: file with mixed ASCII and Cyrillic inside entries", "[zip][edge]")
{
    CTempDir tempDir;
    std::filesystem::path zipPath = tempDir.GetPath() / "mixed_content.zip";

    // File contains a mix of ASCII and UTF-8 Cyrillic bytes
    std::string content = "Title: \xD0\x9F\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82"; // "Привет"
    CreateTestZip(zipPath, {{"mixed.fb2", content}});

    auto data = CZipReader::ReadEntry(zipPath, "mixed.fb2");
    std::string result(data.begin(), data.end());
    REQUIRE(result == content);
}
