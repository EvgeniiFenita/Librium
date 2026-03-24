#include <catch2/catch_test_macros.hpp>
#include "Fb2/Fb2Parser.hpp"
#include "Utils/StringUtils.hpp"

using namespace Librium::Fb2;
using namespace Librium::Utils;

TEST_CASE("FB2 encoding: explicit utf-8 declaration parses correctly", "[fb2][encoding]")
{
    std::string xml = R"(<?xml version="1.0" encoding="utf-8"?>
    <FictionBook>
      <description><title-info>
        <annotation><p>Explicit UTF-8.</p></annotation>
        <keywords>utf8, test</keywords>
      </title-info></description>
    </FictionBook>)";

    auto d = CFb2Parser{}.Parse(xml);
    REQUIRE(d.IsOk());
    REQUIRE(d.annotation == "Explicit UTF-8.");
    REQUIRE(d.keywords == "utf8, test");
}

TEST_CASE("FB2 encoding: no encoding declaration defaults to UTF-8", "[fb2][encoding]")
{
    // XML without any encoding declaration — must be treated as UTF-8 by default.
    std::string xml = R"(<?xml version="1.0"?>
    <FictionBook>
      <description><title-info>
        <annotation><p>No encoding declaration.</p></annotation>
      </title-info></description>
    </FictionBook>)";

    auto d = CFb2Parser{}.Parse(xml);
    REQUIRE(d.IsOk());
    REQUIRE(d.annotation.find("No encoding declaration.") != std::string::npos);
}

TEST_CASE("FB2 encoding: UTF-8 Cyrillic in annotation parsed as valid UTF-8", "[fb2][encoding]")
{
    // "Привет мир" in UTF-8
    std::string cyrillic = "\xD0\x9F\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82 \xD0\xBC\xD0\xB8\xD1\x80";
    std::string xml = "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
                      "<FictionBook><description><title-info>"
                      "<annotation><p>" + cyrillic + "</p></annotation>"
                      "</title-info></description></FictionBook>";

    auto d = CFb2Parser{}.Parse(xml);
    REQUIRE(d.IsOk());
    REQUIRE(CStringUtils::IsUtf8(d.annotation));
    REQUIRE(d.annotation.find(cyrillic) != std::string::npos);
}

TEST_CASE("FB2 encoding: completely missing XML declaration is handled gracefully", "[fb2][encoding]")
{
    // No <?xml?> processing instruction at all.
    std::string xml = "<FictionBook>"
                      "<description><title-info>"
                      "<annotation><p>No prolog.</p></annotation>"
                      "</title-info></description>"
                      "</FictionBook>";

    // Should either parse successfully or return a parse error — must not crash.
    REQUIRE_NOTHROW(CFb2Parser{}.Parse(xml));
    auto d = CFb2Parser{}.Parse(xml);
    // If parsed OK, annotation should be extracted.
    if (d.IsOk())
        REQUIRE(d.annotation.find("No prolog.") != std::string::npos);
}

TEST_CASE("FB2 encoding: uppercase UTF-8 encoding declaration is handled", "[fb2][encoding]")
{
    std::string xml = R"(<?xml version="1.0" encoding="UTF-8"?>
    <FictionBook>
      <description><title-info>
        <annotation><p>Uppercase UTF-8.</p></annotation>
      </title-info></description>
    </FictionBook>)";

    auto d = CFb2Parser{}.Parse(xml);
    REQUIRE(d.IsOk());
    REQUIRE(d.annotation.find("Uppercase UTF-8.") != std::string::npos);
}

#ifdef _WIN32
TEST_CASE("FB2 encoding: windows-1251 encoding declaration triggers CP1251 conversion", "[fb2][encoding]")
{
    // "Тест" in CP1251: Т=D2, е=E5, с=F1, т=F2
    std::string cp1251 = "\xD2\xE5\xF1\xF2";
    std::string xml = "<?xml version=\"1.0\" encoding=\"windows-1251\"?>"
                      "<FictionBook><description><title-info>"
                      "<annotation><p>" + cp1251 + "</p></annotation>"
                      "</title-info></description></FictionBook>";

    auto d = CFb2Parser{}.Parse(xml);
    // After conversion, the annotation should be valid UTF-8
    REQUIRE(CStringUtils::IsUtf8(d.annotation));
    REQUIRE_FALSE(d.annotation.empty());
    // Verify the actual decoded value: CP1251 "\xD2\xE5\xF1\xF2" is "Тест"
    REQUIRE(d.annotation.find("Тест") != std::string::npos);
}
#endif
