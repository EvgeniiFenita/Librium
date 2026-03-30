#include <catch2/catch_test_macros.hpp>
#include "Inpx/InpParser.hpp"
#include "Utils/StringUtils.hpp"
#include "TestUtils.hpp"
#include <filesystem>
#include <sstream>

using namespace Librium::Inpx;
using namespace Librium::Tests;

static std::string CreateInpxContent()
{
    constexpr char SEP = '\x04';
    
    auto inp_line = [SEP](const std::string& authors, const std::string& genres, const std::string& title, 
                          const std::string& series="", const std::string& serno="",
                          const std::string& file_id="100001", const std::string& size="1024000", 
                          const std::string& deleted="0", const std::string& ext="fb2", 
                          const std::string& date="2020-01-01", const std::string& lang="ru",
                          const std::string& rating="", const std::string& keywords="") {
        std::ostringstream ss;
        ss << authors << SEP << genres << SEP << title << SEP << series << SEP << serno << SEP 
           << file_id << SEP << size << SEP << file_id << SEP << deleted << SEP << ext << SEP 
           << date << SEP << lang << SEP << rating << SEP << keywords << SEP << "";
        return ss.str();
    };

    std::ostringstream lines;
    lines << inp_line("Толстой,Лев,Николаевич:", "prose_classic:", "Война и мир", "Эпопея", "1", "100001", "2048000", "0", "fb2", "2020-01-01", "ru", "5") << "\r\n";
    lines << inp_line("Достоевский,Фёдор,Михайлович:", "prose_classic:", "Преступление и наказание", "", "", "100002", "1024000", "0", "fb2", "2020-01-01", "ru", "5") << "\r\n";
    lines << inp_line("Удалённый,Автор,:", "unknown:", "Удалённая книга", "", "", "100003", "512", "1") << "\r\n";
    lines << inp_line("King,Stephen,:", "thriller:", "The Shining", "", "", "100004", "768000", "0", "fb2", "2020-01-01", "en", "4") << "\r\n";
    
    return lines.str();
}

TEST_CASE("InpParser operations", "[inpx]")
{
    CTempDir tempDir;
    std::filesystem::path inpxPath = tempDir.GetPath() / "test.inpx";
    
    CreateTestZip(inpxPath, {
        {"fb2-test-001.zip.inp", CreateInpxContent()},
        {"collection.info", "Test Library\ntest_lib\n65536\nTest\n"},
        {"version.info", "20240101\r\n"}
    });

    SECTION("SAuthor::FullName formatting")
    {
        SAuthor a;
        a.lastName = "Толстой";
        a.firstName = "Лев";
        a.middleName = "Николаевич";
        REQUIRE(a.FullName() == "Толстой, Лев Николаевич");
    }

    SECTION("CInpParser::Parse loads data correctly")
    {
        CInpParser parser;
        auto books = parser.Parse(Librium::Utils::CStringUtils::PathToUtf8String(inpxPath));

        REQUIRE(parser.LastStats().totalLines == 4);
        REQUIRE(parser.LastStats().skippedDeleted == 1);
        REQUIRE(parser.LastStats().parsedOk == 3);

        REQUIRE(books.size() == 3);

        auto findBook = [&](const std::string& title) -> const SBookRecord*
        {
            for (const auto& b : books)
                if (b.title == title) return &b;
            return nullptr;
        };

        const auto* tolstoy = findBook("Война и мир");
        REQUIRE(tolstoy != nullptr);
        REQUIRE(tolstoy->authors.size() == 1);
        REQUIRE(tolstoy->authors[0].lastName == "Толстой");
        REQUIRE(tolstoy->language == "ru");

        const auto* dostoevsky = findBook("Преступление и наказание");
        REQUIRE(dostoevsky != nullptr);
        REQUIRE(dostoevsky->authors[0].lastName == "Достоевский");
        REQUIRE(dostoevsky->language == "ru");

        const auto* king = findBook("The Shining");
        REQUIRE(king != nullptr);
        REQUIRE(king->authors[0].lastName == "King");
        REQUIRE(king->language == "en");
    }
}
