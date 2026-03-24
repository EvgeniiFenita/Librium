#include <catch2/catch_test_macros.hpp>
#include "Utils/StringUtils.hpp"

using namespace Librium::Utils;

// NOTE: Basic SanitizeFilename tests (illegal ASCII chars, Cyrillic preservation,
// mixed Cyrillic+illegal) are already covered in TestStringUtils.cpp.
// This file adds edge cases not present there.

TEST_CASE("CStringUtils::SanitizeFilename — edge cases", "[utils][sanitize]")
{
    SECTION("Leading space is preserved (not a Windows-illegal character)")
    {
        // Leading/trailing spaces are legal in filenames on most systems,
        // though problematic on Windows Explorer. The function only replaces
        // the nine Windows-illegal characters (\ / : * ? " < > |).
        std::string result = CStringUtils::SanitizeFilename(" Book.fb2");
        // Space is NOT in the illegal set, so it should pass through unchanged.
        REQUIRE(result == " Book.fb2");
    }

    SECTION("Trailing space is preserved")
    {
        std::string result = CStringUtils::SanitizeFilename("Book.fb2 ");
        REQUIRE(result == "Book.fb2 ");
    }

    SECTION("All nine Windows-illegal ASCII chars replaced in one string")
    {
        // \ / : * ? " < > |  →  all become _
        std::string input    = "a\\b/c:d*e?f\"g<h>i|j";
        std::string expected = "a_b_c_d_e_f_g_h_i_j";
        REQUIRE(CStringUtils::SanitizeFilename(input) == expected);
    }

    SECTION("Tab character is preserved (not Windows-illegal)")
    {
        std::string input = "Book\tTitle.fb2";
        REQUIRE(CStringUtils::SanitizeFilename(input) == input);
    }

    SECTION("Very long filename (300 chars) is not truncated")
    {
        // SanitizeFilename only replaces illegal chars, it does not enforce
        // OS filename length limits — that is the caller's responsibility.
        std::string longName(300, 'A');
        longName += ".fb2";
        std::string result = CStringUtils::SanitizeFilename(longName);
        REQUIRE(result.size() == longName.size());
        REQUIRE(result == longName);
    }

    SECTION("Very long filename containing illegal chars — all replaced, length preserved")
    {
        std::string longName(298, 'A');
        longName += ":.fb2"; // two extra chars: ':' (illegal) + '.fb2'
        std::string result = CStringUtils::SanitizeFilename(longName);
        REQUIRE(result.size() == longName.size());
        REQUIRE(result.find(':') == std::string::npos);
    }

    SECTION("String consisting entirely of illegal characters")
    {
        std::string result = CStringUtils::SanitizeFilename("\\/:*?\"<>|");
        REQUIRE(result == "_________");
    }

    SECTION("Digits and punctuation (not illegal) are untouched")
    {
        std::string input = "Book 1 - Vol. 2 (2024).fb2";
        REQUIRE(CStringUtils::SanitizeFilename(input) == input);
    }

    SECTION("Single illegal character at start")
    {
        REQUIRE(CStringUtils::SanitizeFilename("/Book.fb2") == "_Book.fb2");
    }

    SECTION("Single illegal character at end")
    {
        REQUIRE(CStringUtils::SanitizeFilename("Book.fb2|") == "Book.fb2_");
    }

    SECTION("CJK characters are preserved (multi-byte UTF-8 sequence)")
    {
        // U+4E2D (中) = E4 B8 AD
        std::string input = "\xE4\xB8\xAD\xE6\x96\x87.fb2";
        REQUIRE(CStringUtils::SanitizeFilename(input) == input);
    }
}
