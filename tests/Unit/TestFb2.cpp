#include <catch2/catch_test_macros.hpp>
#include "Fb2/Fb2Parser.hpp"

using namespace LibIndexer::Fb2;

static const std::string k_fb2 = R"(<?xml version="1.0" encoding="utf-8"?>
<FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"
             xmlns:l="http://www.w3.org/1999/xlink">
  <description>
    <title-info>
      <annotation><p>First.</p><p>Second.</p></annotation>
      <keywords>history, war</keywords>
      <coverpage><image l:href="#cover.jpg"/></coverpage>
    </title-info>
    <publish-info>
      <publisher>Test Publisher</publisher>
      <year>1978</year>
      <isbn>978-0-00-000000-0</isbn>
    </publish-info>
  </description>
  <binary id="cover.jpg" content-type="image/jpeg">/9j/4AAQSkZJRgABAQEASABIAAD/2Q==</binary>
</FictionBook>)";

TEST_CASE("Parses annotation", "[fb2]")
{
    auto d = CFb2Parser{}.Parse(k_fb2);
    REQUIRE(d.IsOk());
    REQUIRE(d.annotation.find("First") != std::string::npos);
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
TEST_CASE("Extracts cover", "[fb2]")
{
    auto d = CFb2Parser{}.Parse(k_fb2);
    REQUIRE(d.coverMime == "image/jpeg");
    REQUIRE_FALSE(d.coverData.empty());
}
TEST_CASE("Invalid XML returns parseError", "[fb2]")
{
    REQUIRE_FALSE(CFb2Parser{}.Parse("not xml <<<").IsOk());
}
TEST_CASE("Empty input returns parseError", "[fb2]")
{
    REQUIRE_FALSE(CFb2Parser{}.Parse(std::vector<uint8_t>{}).IsOk());
}
