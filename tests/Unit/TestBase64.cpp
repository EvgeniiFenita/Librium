#include <catch2/catch_test_macros.hpp>

#include "Utils/Base64.hpp"

#include <string>

using namespace Librium::Utils;

TEST_CASE("CBase64 encode", "[base64]")
{
    SECTION("Known encoding: 'hello' -> 'aGVsbG8='")
    {
        REQUIRE(CBase64::Encode("hello") == "aGVsbG8=");
    }

    SECTION("Known encoding: 'Man' -> 'TWFu' (no padding)")
    {
        REQUIRE(CBase64::Encode("Man") == "TWFu");
    }

    SECTION("Known encoding: 'Ma' -> 'TWE=' (single padding)")
    {
        REQUIRE(CBase64::Encode("Ma") == "TWE=");
    }

    SECTION("Known encoding: 'M' -> 'TQ==' (double padding)")
    {
        REQUIRE(CBase64::Encode("M") == "TQ==");
    }

    SECTION("Empty string encodes to empty string")
    {
        REQUIRE(CBase64::Encode("") == "");
    }

    SECTION("Output contains only valid base64 chars")
    {
        std::string encoded = CBase64::Encode("any test data 123");
        for (char c : encoded)
        {
            bool valid = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                         (c >= '0' && c <= '9') || c == '+' || c == '/' || c == '=';
            REQUIRE(valid);
        }
    }
}

TEST_CASE("CBase64 decode", "[base64]")
{
    SECTION("Known decoding: 'aGVsbG8=' -> 'hello'")
    {
        REQUIRE(CBase64::Decode("aGVsbG8=") == "hello");
    }

    SECTION("Known decoding: 'TWFu' -> 'Man'")
    {
        REQUIRE(CBase64::Decode("TWFu") == "Man");
    }

    SECTION("Empty string decodes to empty string")
    {
        REQUIRE(CBase64::Decode("") == "");
    }
}

TEST_CASE("CBase64 roundtrip", "[base64]")
{
    SECTION("Short ASCII string")
    {
        std::string original = "Hello, World!";
        REQUIRE(CBase64::Decode(CBase64::Encode(original)) == original);
    }

    SECTION("Long text")
    {
        std::string original = "The quick brown fox jumps over the lazy dog. "
                               "Pack my box with five dozen liquor jugs.";
        REQUIRE(CBase64::Decode(CBase64::Encode(original)) == original);
    }

    SECTION("Cyrillic UTF-8 text")
    {
        // "Война и мир" in UTF-8
        std::string original = "\xD0\x92\xD0\xBE\xD0\xB9\xD0\xBD\xD0\xB0 "
                               "\xD0\xB8 \xD0\xBC\xD0\xB8\xD1\x80";
        REQUIRE(CBase64::Decode(CBase64::Encode(original)) == original);
    }

    SECTION("All byte values 0-255")
    {
        std::string binary;
        binary.reserve(256);
        for (int i = 0; i < 256; ++i)
            binary += static_cast<char>(i);
        REQUIRE(CBase64::Decode(CBase64::Encode(binary)) == binary);
    }

    SECTION("Single byte - length 1")
    {
        REQUIRE(CBase64::Decode(CBase64::Encode("A")) == "A");
    }

    SECTION("Two bytes - length 2")
    {
        REQUIRE(CBase64::Decode(CBase64::Encode("AB")) == "AB");
    }

    SECTION("Three bytes - length 3 (no padding)")
    {
        REQUIRE(CBase64::Decode(CBase64::Encode("ABC")) == "ABC");
    }

    SECTION("Null byte preserved in roundtrip")
    {
        // Construct with explicit null inside: "before\x00after" (12 bytes)
        std::string withNull = std::string("before") + '\0' + std::string("after");
        REQUIRE(withNull.size() == 12);
        REQUIRE(CBase64::Decode(CBase64::Encode(withNull)) == withNull);
    }
}
