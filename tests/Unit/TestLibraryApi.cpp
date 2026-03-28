#include <catch2/catch_test_macros.hpp>
#include "Service/LibraryApi.hpp"
#include "Service/AppService.hpp"
#include "Config/AppConfig.hpp"
#include "Database/Database.hpp"
#include "Database/QueryTypes.hpp"
#include "Inpx/BookRecord.hpp"
#include "Utils/StringUtils.hpp"
#include "TestUtils.hpp"
#include <filesystem>
#include <fstream>
#include <string>
#include <optional>

using namespace Librium;
using namespace Librium::Tests;

namespace {

Inpx::SBookRecord MakeApiRec(const std::string& id, const std::string& title)
{
    Inpx::SBookRecord r;
    r.libId       = id;
    r.title       = title;
    r.archiveName = "api_arch";
    r.fileName    = id;
    r.fileExt     = "fb2";
    r.language    = "ru";
    r.genres      = {"sf"};
    r.authors.push_back({"ApiAuthor", "Test", ""});
    return r;
}

} // anonymous namespace

TEST_CASE("CLibraryApi::GetStats on empty database returns zeros", "[libraryapi]")
{
    CTempDir tempDir;
    Config::SAppConfig cfg;
    cfg.database.path = (tempDir.GetPath() / "api_test.db").string();

    Service::CLibraryApi api(cfg);
    const auto stats = api.GetStats();

    REQUIRE(stats.totalBooks == 0);
    REQUIRE(stats.totalAuthors == 0);
}

TEST_CASE("CLibraryApi::GetStats after inserting books returns correct count", "[libraryapi]")
{
    CTempDir tempDir;
    Config::SAppConfig cfg;
    cfg.database.path = (tempDir.GetPath() / "api_stats.db").string();

    {
        Db::CDatabase db(cfg.database.path);
        (void)db.InsertBook(MakeApiRec("1001", "Book One"));
        (void)db.InsertBook(MakeApiRec("1002", "Book Two"));
    }

    Service::CLibraryApi api(cfg);
    const auto stats = api.GetStats();

    REQUIRE(stats.totalBooks == 2);
    REQUIRE(stats.totalAuthors >= 1);
}

TEST_CASE("CLibraryApi::SearchBooks on empty database returns empty result", "[libraryapi]")
{
    CTempDir tempDir;
    Config::SAppConfig cfg;
    cfg.database.path = (tempDir.GetPath() / "api_search.db").string();

    Service::CLibraryApi api(cfg);
    Db::SQueryParams p;
    const auto result = api.SearchBooks(p);

    REQUIRE(result.totalFound == 0);
    REQUIRE(result.books.empty());
}

TEST_CASE("CLibraryApi::GetBook for non-existent ID returns nullopt", "[libraryapi]")
{
    CTempDir tempDir;
    Config::SAppConfig cfg;
    cfg.database.path = (tempDir.GetPath() / "api_getbook.db").string();

    Service::CLibraryApi api(cfg);
    const auto book = api.GetBook(99999);

    REQUIRE_FALSE(book.has_value());
}

TEST_CASE("CLibraryApi::GetBook for existing book returns correct data", "[libraryapi]")
{
    CTempDir tempDir;
    Config::SAppConfig cfg;
    cfg.database.path = (tempDir.GetPath() / "api_existing.db").string();

    int64_t insertedId = 0;
    const std::string expectedTitle = "Existing Book";

    {
        Db::CDatabase db(cfg.database.path);
        insertedId = db.InsertBook(MakeApiRec("2001", expectedTitle));
    }

    Service::CLibraryApi api(cfg);
    const auto book = api.GetBook(insertedId);

    REQUIRE(book.has_value());
    REQUIRE(book->book.title == expectedTitle);
    REQUIRE(book->coverPath.empty());
}

TEST_CASE("CLibraryApi::GetBook with cover file returns coverPath", "[libraryapi]")
{
    CTempDir tempDir;
    Config::SAppConfig cfg;
    cfg.database.path = (tempDir.GetPath() / "api_cover.db").string();

    int64_t insertedId = 0;

    {
        Db::CDatabase db(cfg.database.path);
        insertedId = db.InsertBook(MakeApiRec("3001", "Cover Book"));
    }

    const auto metaDir = Config::CAppPaths::GetBookMetaDir(
        Utils::CStringUtils::Utf8ToPath(cfg.database.path), insertedId);
    std::filesystem::create_directories(metaDir);

    {
        std::ofstream coverFile(metaDir / "cover.jpg");
        coverFile << "fake cover data";
    }

    Service::CLibraryApi api(cfg);
    const auto book = api.GetBook(insertedId);

    REQUIRE(book.has_value());
    REQUIRE_FALSE(book->coverPath.empty());
}

TEST_CASE("CLibraryApi::SearchBooks returns books after insert", "[libraryapi]")
{
    CTempDir tempDir;
    Config::SAppConfig cfg;
    cfg.database.path = (tempDir.GetPath() / "api_search3.db").string();

    {
        Db::CDatabase db(cfg.database.path);
        (void)db.InsertBook(MakeApiRec("4001", "Alpha"));
        (void)db.InsertBook(MakeApiRec("4002", "Beta"));
        (void)db.InsertBook(MakeApiRec("4003", "Gamma"));
    }

    Service::CLibraryApi api(cfg);
    const auto result = api.SearchBooks(Db::SQueryParams{});

    REQUIRE(result.totalFound == 3);
}

TEST_CASE("CLibraryApi::ExportBook for non-existent book throws", "[libraryapi]")
{
    CTempDir tempDir;
    Config::SAppConfig cfg;
    cfg.database.path = (tempDir.GetPath() / "api_export.db").string();

    Service::CLibraryApi api(cfg);

    REQUIRE_THROWS(api.ExportBook(99999, tempDir.GetPath() / "out"));
}

TEST_CASE("CLibraryApi::ExportBook extracts file from ZIP to output directory", "[libraryapi]")
{
    CTempDir tempDir;
    Config::SAppConfig cfg;
    cfg.database.path   = (tempDir.GetPath() / "export_test.db").string();
    cfg.library.archivesDir = tempDir.GetPath().string();

    const std::string archiveStem = "export_arch";
    const std::string fileBaseName = "book_export";
    const std::string fileName    = fileBaseName + ".fb2";
    const std::string fb2Content  = R"(<?xml version="1.0"?><FictionBook/>)";

    // Create ZIP archive with an FB2 file inside
    const auto zipPath = tempDir.GetPath() / (archiveStem + ".zip");
    CreateTestZip(zipPath, {{fileName, fb2Content}});

    // Insert book record pointing to that archive
    Inpx::SBookRecord rec;
    rec.libId       = "EXP01";
    rec.title       = "Export Test";
    rec.archiveName = archiveStem;
    rec.fileName    = fileBaseName;
    rec.fileExt     = "fb2";
    rec.language    = "en";
    rec.genres      = {"test"};
    rec.authors.push_back({"ExportAuthor", "Test", ""});

    int64_t bookId = 0;
    {
        Db::CDatabase db(cfg.database.path);
        bookId = db.InsertBook(rec);
    }
    REQUIRE(bookId > 0);

    // Export the book
    const auto outDir = tempDir.GetPath() / "exported";
    Service::CLibraryApi api(cfg);
    const auto outPath = api.ExportBook(bookId, outDir);

    // Verify output file exists and has expected content
    REQUIRE(std::filesystem::exists(outPath));
    REQUIRE(outPath.filename().string() == fileName);

    std::ifstream ifs(outPath, std::ios::binary);
    REQUIRE(ifs.is_open());
    const std::string actualContent((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    REQUIRE(actualContent == fb2Content);
}

TEST_CASE("CLibraryApi::ExportBook creates output directory if it does not exist", "[libraryapi]")
{
    CTempDir tempDir;
    Config::SAppConfig cfg;
    cfg.database.path    = (tempDir.GetPath() / "export_mkdir.db").string();
    cfg.library.archivesDir = tempDir.GetPath().string();

    const std::string archiveStem = "mkdir_arch";
    const std::string fileBaseName = "mkdir_book";
    const std::string fileName    = fileBaseName + ".fb2";

    const auto zipPath = tempDir.GetPath() / (archiveStem + ".zip");
    CreateTestZip(zipPath, {{fileName, "<FictionBook/>"}});

    Inpx::SBookRecord rec;
    rec.libId       = "MK01";
    rec.title       = "Mkdir Test";
    rec.archiveName = archiveStem;
    rec.fileName    = fileBaseName;
    rec.fileExt     = "fb2";
    rec.language    = "en";

    int64_t bookId = 0;
    {
        Db::CDatabase db(cfg.database.path);
        bookId = db.InsertBook(rec);
    }

    const auto deepOutDir = tempDir.GetPath() / "deep" / "nested" / "out";
    REQUIRE_FALSE(std::filesystem::exists(deepOutDir));

    Service::CLibraryApi api(cfg);
    const auto outPath = api.ExportBook(bookId, deepOutDir);

    REQUIRE(std::filesystem::exists(deepOutDir));
    REQUIRE(std::filesystem::exists(outPath));
}
