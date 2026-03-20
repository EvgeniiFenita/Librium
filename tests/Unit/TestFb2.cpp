#include <catch2/catch_test_macros.hpp>
#include "Fb2/Fb2Parser.hpp"
#include "Utils/StringUtils.hpp"

using namespace Librium::Fb2;

static const std::string k_fb2 = R"(<?xml version="1.0" encoding="utf-8"?>
<FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"
             xmlns:l="http://www.w3.org/1999/xlink">
  <description>
    <title-info>
      <annotation><p>First.</p><p>Second.</p></annotation>
      <keywords>history, war</keywords>
    </title-info>
    <publish-info>
      <publisher>Test Publisher</publisher>
      <year>1978</year>
      <isbn>978-0-00-000000-0</isbn>
    </publish-info>
  </description>
</FictionBook>)";

TEST_CASE("Parses annotation", "[fb2]")
{
    auto d = CFb2Parser{}.Parse(k_fb2);
    REQUIRE(d.IsOk());
    REQUIRE(d.annotation.find("First.") != std::string::npos);
    REQUIRE(d.annotation.find("Second.") != std::string::npos);
}

TEST_CASE("Parses keywords", "[fb2]")
{
    REQUIRE(CFb2Parser{}.Parse(k_fb2).keywords == "history, war");
}

TEST_CASE("Parses publish info", "[fb2]")
{
    auto d = CFb2Parser{}.Parse(k_fb2);
    REQUIRE(d.publisher == "Test Publisher");
    REQUIRE(d.isbn == "978-0-00-000000-0");
    REQUIRE(d.publishYear == "1978");
}

TEST_CASE("Invalid XML returns parseError", "[fb2]")
{
    REQUIRE_FALSE(CFb2Parser{}.Parse("not xml <<<").IsOk());
}

TEST_CASE("Empty input returns empty data", "[fb2]")
{
    auto d = CFb2Parser{}.Parse(std::vector<uint8_t>{});
    REQUIRE(d.annotation.empty());
}

TEST_CASE("Parses nested tags in annotation", "[fb2]")
{
    std::string xml = R"(<?xml version="1.0" encoding="utf-8"?>
    <FictionBook>
      <description><title-info><annotation>
        <p>Text <strong>bold</strong> and <a href="#link">link</a>.</p>
        <empty-line/>
        <p>Next line.</p>
      </annotation></title-info></description>
    </FictionBook>)";
    auto d = CFb2Parser{}.Parse(xml);
    // Should extract raw text without tags, preserving structure
    REQUIRE(d.annotation == "Text bold and link.\n\nNext line.");
}

TEST_CASE("Handles XML namespaces in tags", "[fb2]")
{
    std::string xml = R"(<?xml version="1.0" encoding="utf-8"?>
    <fb:FictionBook xmlns:fb="http://www.gribuser.ru/xml/fictionbook/2.0">
      <fb:description>
        <fb:title-info>
          <fb:annotation><p>Namespaced text.</p></fb:annotation>
          <fb:keywords>space, stars</fb:keywords>
        </fb:title-info>
      </fb:description>
    </fb:FictionBook>)";
    auto d = CFb2Parser{}.Parse(xml);
    REQUIRE(d.annotation == "Namespaced text.");
    REQUIRE(d.keywords == "space, stars");
}

TEST_CASE("Parses cover image", "[fb2]")
{
    std::string b64 = "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mNkYAAAAAYAAjCB0C8AAAAASUVORK5CYII=";
    std::string xml = R"(<?xml version="1.0" encoding="utf-8"?>
    <FictionBook xmlns:l="http://www.w3.org/1999/xlink">
      <description><title-info>
        <coverpage><image l:href="#cover.png"/></coverpage>
      </title-info></description>
      <binary id="cover.png" content-type="image/png">
        )" + b64 + R"(
      </binary>
    </FictionBook>)";
    
    auto d = CFb2Parser{}.Parse(xml);
    REQUIRE(d.coverExt == ".png");
    REQUIRE(d.coverData.size() == 68);
}

#ifdef _WIN32
TEST_CASE("Converts Windows-1251 encoding", "[fb2]")
{
    // "Привет" in CP1251 is \xcf \xf0 \xe8 \xe2 \xe5 \xf2
    std::string cp1251Text = "\xcf\xf0\xe8\xe2\xe5\xf2"; 
    std::string xml = "<?xml version=\"1.0\" encoding=\"windows-1251\"?><FictionBook><description><title-info><annotation><p>" + cp1251Text + "</p></annotation></title-info></description></FictionBook>";
    
    auto d = CFb2Parser{}.Parse(xml);
    
    // In UTF-8 "Привет" is \xd0\x9f\xd1\x80\xd0\xb8\xd0\xb2\xd0\xb5\xd1\x82
    std::string utf8Text = "\xd0\x9f\xd1\x80\xd0\xb8\xd0\xb2\xd0\xb5\xd1\x82";
    REQUIRE(d.annotation == utf8Text);
}
#endif
