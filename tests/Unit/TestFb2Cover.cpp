#include <catch2/catch_test_macros.hpp>
#include "Fb2/Fb2Parser.hpp"

using namespace Librium::Fb2;

// Minimal valid 1x1 JPEG (base64-encoded)
static const std::string k_tinyJpegB64 =
    "/9j/4AAQSkZJRgABAQEASABIAAD/2wBDAP//////////////////////"
    "////////////////////////////////////////////////////////////////"
    "wgALCAABAAEBAREA/8QAFBABAAAAAAAAAAAAAAAAAAAAAP/aAAgBAQABPxAA/9k=";

// Minimal 1x1 PNG (base64-encoded) -- same as used in TestFb2.cpp
static const std::string k_tinyPngB64 =
    "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mNkYAAAAAYAAjCB0C8AAAAASUVORK5CYII=";

TEST_CASE("FB2 cover: JPEG content-type produces .jpg extension", "[fb2][cover]")
{
    std::string xml = R"(<?xml version="1.0" encoding="utf-8"?>
    <FictionBook xmlns:l="http://www.w3.org/1999/xlink">
      <description><title-info>
        <coverpage><image l:href="#cover.jpg"/></coverpage>
      </title-info></description>
      <binary id="cover.jpg" content-type="image/jpeg">
        )" + k_tinyJpegB64 + R"(
      </binary>
    </FictionBook>)";

    auto d = CFb2Parser{}.Parse(xml);
    REQUIRE(d.IsOk());
    REQUIRE(d.coverExt == ".jpg");
    REQUIRE_FALSE(d.coverData.empty());
}

TEST_CASE("FB2 cover: PNG content-type produces .png extension", "[fb2][cover]")
{
    std::string xml = R"(<?xml version="1.0" encoding="utf-8"?>
    <FictionBook xmlns:l="http://www.w3.org/1999/xlink">
      <description><title-info>
        <coverpage><image l:href="#cover.png"/></coverpage>
      </title-info></description>
      <binary id="cover.png" content-type="image/png">
        )" + k_tinyPngB64 + R"(
      </binary>
    </FictionBook>)";

    auto d = CFb2Parser{}.Parse(xml);
    REQUIRE(d.IsOk());
    REQUIRE(d.coverExt == ".png");
    REQUIRE_FALSE(d.coverData.empty());
}

TEST_CASE("FB2 cover: no coverpage tag yields empty cover data", "[fb2][cover]")
{
    std::string xml = R"(<?xml version="1.0" encoding="utf-8"?>
    <FictionBook xmlns:l="http://www.w3.org/1999/xlink">
      <description><title-info>
        <annotation><p>A book without a cover.</p></annotation>
      </title-info></description>
      <body><p>Content</p></body>
    </FictionBook>)";

    auto d = CFb2Parser{}.Parse(xml);
    REQUIRE(d.IsOk());
    REQUIRE(d.coverData.empty());
    REQUIRE(d.coverExt.empty());
}

TEST_CASE("FB2 cover: binary tag present but no coverpage reference yields empty cover", "[fb2][cover]")
{
    // A <binary> section exists but <coverpage> does not reference it as the cover image.
    std::string xml = R"(<?xml version="1.0" encoding="utf-8"?>
    <FictionBook xmlns:l="http://www.w3.org/1999/xlink">
      <description><title-info>
        <annotation><p>Book with orphan binary.</p></annotation>
      </title-info></description>
      <binary id="image.png" content-type="image/png">
        )" + k_tinyPngB64 + R"(
      </binary>
    </FictionBook>)";

    auto d = CFb2Parser{}.Parse(xml);
    REQUIRE(d.IsOk());
    // No <coverpage> -- parser should not extract cover
    REQUIRE(d.coverData.empty());
}

TEST_CASE("FB2 cover: invalid base64 in binary does not crash", "[fb2][cover]")
{
    // Base64::Decode stops at the first non-base64 character.
    // A string starting with '!' is entirely invalid -- Decode returns empty string.
    std::string xml = R"(<?xml version="1.0" encoding="utf-8"?>
    <FictionBook xmlns:l="http://www.w3.org/1999/xlink">
      <description><title-info>
        <coverpage><image l:href="#cover.png"/></coverpage>
      </title-info></description>
      <binary id="cover.png" content-type="image/png">
        !!!NOT_VALID_BASE64_AT_ALL!!!
      </binary>
    </FictionBook>)";

    auto d = CFb2Parser{}.Parse(xml);
    REQUIRE(d.IsOk());
    // All-invalid base64 (starts with '!') decodes to empty
    REQUIRE(d.coverData.empty());
}

TEST_CASE("FB2 cover: multiple binary sections -- cover-image binary is selected", "[fb2][cover]")
{
    // There is a non-cover image and a cover image.
    // The parser should pick the one referenced by <coverpage>.
    std::string xml = R"(<?xml version="1.0" encoding="utf-8"?>
    <FictionBook xmlns:l="http://www.w3.org/1999/xlink">
      <description><title-info>
        <coverpage><image l:href="#the-cover.jpg"/></coverpage>
      </title-info></description>
      <binary id="inline-image.png" content-type="image/png">
        )" + k_tinyPngB64 + R"(
      </binary>
      <binary id="the-cover.jpg" content-type="image/jpeg">
        )" + k_tinyJpegB64 + R"(
      </binary>
    </FictionBook>)";

    auto d = CFb2Parser{}.Parse(xml);
    REQUIRE(d.IsOk());
    REQUIRE(d.coverExt == ".jpg");
    REQUIRE_FALSE(d.coverData.empty());
    // Verify the JPEG binary was selected and not the PNG.
    // JPEG files always begin with magic bytes 0xFF 0xD8.
    REQUIRE(static_cast<unsigned char>(d.coverData[0]) == 0xFF);
    REQUIRE(static_cast<unsigned char>(d.coverData[1]) == 0xD8);
}