#include <catch2/catch_test_macros.hpp>

#include "Utils/StringUtils.hpp"

using namespace Librium::Utils;

TEST_CASE("CStringUtils::IsUtf8", "[utils]")
{
    SECTION("Valid ASCII string")
    {
        REQUIRE(CStringUtils::IsUtf8("hello world"));
    }

    SECTION("Empty string is valid UTF-8")
    {
        REQUIRE(CStringUtils::IsUtf8(""));
    }

    SECTION("Valid Cyrillic UTF-8")
    {
        // "Привет мир" - proper UTF-8 encoded Cyrillic
        REQUIRE(CStringUtils::IsUtf8("\xD0\x9F\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82"));
    }

    SECTION("Valid 3-byte UTF-8 sequence (CJK)")
    {
        // U+4E2D (中) = E4 B8 AD
        REQUIRE(CStringUtils::IsUtf8("\xE4\xB8\xAD"));
    }

    SECTION("Valid 4-byte UTF-8 emoji")
    {
        // U+1F600 (😀) = F0 9F 98 80
        REQUIRE(CStringUtils::IsUtf8("\xF0\x9F\x98\x80"));
    }

    SECTION("Invalid: lone continuation byte")
    {
        REQUIRE_FALSE(CStringUtils::IsUtf8("\x80"));
    }

    SECTION("Invalid: overlong 2-byte encoding of NUL (C0 80)")
    {
        REQUIRE_FALSE(CStringUtils::IsUtf8("\xC0\x80"));
    }

    SECTION("Invalid: overlong 2-byte encoding of ASCII slash (C0 AF)")
    {
        REQUIRE_FALSE(CStringUtils::IsUtf8("\xC0\xAF"));
    }

    SECTION("Invalid: truncated 2-byte sequence")
    {
        // Start of 2-byte sequence but missing continuation
        REQUIRE_FALSE(CStringUtils::IsUtf8("\xC2"));
    }

    SECTION("Invalid: truncated 3-byte sequence")
    {
        // E4 B8 is only 2 of the 3 needed bytes for U+4E2D
        REQUIRE_FALSE(CStringUtils::IsUtf8("\xE4\xB8"));
    }

    SECTION("Invalid: truncated 4-byte sequence")
    {
        // F0 9F 98 is only 3 of the 4 needed bytes
        REQUIRE_FALSE(CStringUtils::IsUtf8("\xF0\x9F\x98"));
    }

    SECTION("Invalid: raw CP1251 Cyrillic bytes are not valid UTF-8")
    {
        // Raw CP1251 Cyrillic bytes form an invalid UTF-8 sequence: 0xCF is a 2-byte lead byte,
        // but 0xF0 is another lead byte (not a continuation byte), making the sequence malformed.
        REQUIRE_FALSE(CStringUtils::IsUtf8("\xCF\xF0\xE8\xE2\xE5\xF2"));
    }

    SECTION("Mixed valid ASCII and valid UTF-8 multibyte")
    {
        // "OK: " followed by U+2713 (✓) = E2 9C 93
        REQUIRE(CStringUtils::IsUtf8("OK: \xE2\x9C\x93"));
    }
}

TEST_CASE("CStringUtils::Cp1251ToUtf8", "[utils]")
{
    SECTION("Pure ASCII is returned unchanged")
    {
        REQUIRE(CStringUtils::Cp1251ToUtf8("hello") == "hello");
    }

    SECTION("Empty input returns empty output")
    {
        REQUIRE(CStringUtils::Cp1251ToUtf8("").empty());
    }

    // On Linux, Cp1251ToUtf8 is a no-op (returns input unchanged).
    // The conversion tests are Windows-only where the real implementation runs.
#ifdef _WIN32
    SECTION("Converts CP1251 Cyrillic to valid UTF-8")
    {
        // CP1251: "Привет" — П=CF, р=F0, и=E8, в=E2, е=E5, т=F2
        std::string cp1251 = "\xCF\xF0\xE8\xE2\xE5\xF2";
        std::string result = CStringUtils::Cp1251ToUtf8(cp1251);
        REQUIRE(CStringUtils::IsUtf8(result));
        REQUIRE_FALSE(result.empty());
    }

    SECTION("Result of conversion is valid UTF-8")
    {
        // "Война" (War) in CP1251: В=C2, о=EE, й=E9, н=ED, а=E0
        std::string cp1251 = "\xC2\xEE\xE9\xED\xE0";
        std::string result = CStringUtils::Cp1251ToUtf8(cp1251);
        REQUIRE(CStringUtils::IsUtf8(result));
    }
#endif

    SECTION("CP1251 digits and punctuation pass through correctly")
    {
        std::string cp1251 = "1234!@#";
        REQUIRE(CStringUtils::Cp1251ToUtf8(cp1251) == "1234!@#");
    }
}

TEST_CASE("CStringUtils::Utf16ToUtf8", "[utils]")
{
    SECTION("Empty wstring returns empty string")
    {
        REQUIRE(CStringUtils::Utf16ToUtf8(L"").empty());
    }

    SECTION("ASCII wstring converts correctly")
    {
        REQUIRE(CStringUtils::Utf16ToUtf8(L"hello") == "hello");
    }

    SECTION("ASCII with numbers and symbols")
    {
        REQUIRE(CStringUtils::Utf16ToUtf8(L"ABC 123 !@#") == "ABC 123 !@#");
    }

    SECTION("Cyrillic wstring produces valid UTF-8")
    {
        std::wstring input = L"\u041F\u0440\u0438\u0432\u0435\u0442"; // "Привет"
        std::string result = CStringUtils::Utf16ToUtf8(input);
        REQUIRE(CStringUtils::IsUtf8(result));
        REQUIRE_FALSE(result.empty());
        // UTF-8 "Привет" is 12 bytes (2 bytes per Cyrillic char)
        REQUIRE(result.size() == 12);
    }

    SECTION("Roundtrip: ASCII utf16 -> utf8 is identical to original")
    {
        std::string original = "The quick brown fox";
        std::wstring wide(original.begin(), original.end());
        REQUIRE(CStringUtils::Utf16ToUtf8(wide) == original);
    }
}
