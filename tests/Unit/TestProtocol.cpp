#include <catch2/catch_test_macros.hpp>
#include "Protocol/JsonProtocol.hpp"
#include "Service/AppService.hpp"
#include "Config/AppConfig.hpp"
#include "Database/Database.hpp"
#include "Utils/Base64.hpp"
#include "TestUtils.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <filesystem>
#include <string>

using namespace Librium;
using namespace Librium::Tests;

namespace {

nlohmann::json DecodeResponse(const std::string& base64Response)
{
    std::string jsonStr = Utils::CBase64::Decode(base64Response);
    return nlohmann::json::parse(jsonStr);
}

} // anonymous namespace

TEST_CASE("CJsonProtocol::BuildErrorResponse encodes error correctly", "[protocol]")
{
    std::string result = Protocol::CJsonProtocol::BuildErrorResponse("test error message");
    nlohmann::json json = DecodeResponse(result);

    REQUIRE(json["status"] == "error");
    REQUIRE(json["error"] == "test error message");
}

TEST_CASE("CJsonProtocol::BuildErrorResponse with empty message", "[protocol]")
{
    std::string result = Protocol::CJsonProtocol::BuildErrorResponse("");
    nlohmann::json json = DecodeResponse(result);

    REQUIRE(json["status"] == "error");
    REQUIRE(json.contains("error"));
}

TEST_CASE("CJsonProtocol::Process with invalid Base64 returns error", "[protocol]")
{
    CTempDir tempDir;
    Config::SAppConfig cfg;
    cfg.database.path = (tempDir.GetPath() / "test.db").string();

    Service::CAppService service(cfg);
    std::string result = Protocol::CJsonProtocol::Process("THIS_IS_NOT_VALID_BASE64!!!", service, nullptr);
    nlohmann::json json = DecodeResponse(result);

    REQUIRE(json["status"] == "error");
    REQUIRE(json.contains("error"));
}

TEST_CASE("CJsonProtocol::Process with malformed JSON returns error", "[protocol]")
{
    CTempDir tempDir;
    Config::SAppConfig cfg;
    cfg.database.path = (tempDir.GetPath() / "test.db").string();

    Service::CAppService service(cfg);
    std::string encoded = Utils::CBase64::Encode("{not valid json");
    std::string result = Protocol::CJsonProtocol::Process(encoded, service, nullptr);
    nlohmann::json json = DecodeResponse(result);

    REQUIRE(json["status"] == "error");
}

TEST_CASE("CJsonProtocol::Process with missing action field returns error", "[protocol]")
{
    CTempDir tempDir;
    Config::SAppConfig cfg;
    cfg.database.path = (tempDir.GetPath() / "test.db").string();

    Service::CAppService service(cfg);
    std::string request = R"({"params": {"limit": 10}})";
    std::string encoded = Utils::CBase64::Encode(request);
    std::string result = Protocol::CJsonProtocol::Process(encoded, service, nullptr);
    nlohmann::json json = DecodeResponse(result);

    REQUIRE(json["status"] == "error");
    REQUIRE(json.contains("error"));

    std::string lowerErr = json["error"].get<std::string>();
    std::transform(lowerErr.begin(), lowerErr.end(), lowerErr.begin(), ::tolower);
    REQUIRE((lowerErr.find("action") != std::string::npos || lowerErr.find("missing") != std::string::npos));
}

TEST_CASE("CJsonProtocol::Process with unknown action returns error", "[protocol]")
{
    CTempDir tempDir;
    Config::SAppConfig cfg;
    cfg.database.path = (tempDir.GetPath() / "test.db").string();

    Service::CAppService service(cfg);
    std::string request = R"({"action": "nonexistent_action_xyz", "params": {}})";
    std::string encoded = Utils::CBase64::Encode(request);
    std::string result = Protocol::CJsonProtocol::Process(encoded, service, nullptr);
    nlohmann::json json = DecodeResponse(result);

    REQUIRE(json["status"] == "error");
    std::string errorMsg = json["error"].get<std::string>();
    REQUIRE((errorMsg.find("Unknown") != std::string::npos || errorMsg.find("nonexistent_action_xyz") != std::string::npos));
}

TEST_CASE("CJsonProtocol::Process with valid stats action returns ok", "[protocol]")
{
    CTempDir tempDir;
    Config::SAppConfig cfg;
    cfg.database.path = (tempDir.GetPath() / "test.db").string();

    Service::CAppService service(cfg);
    std::string request = R"({"action": "stats", "params": {}})";
    std::string encoded = Utils::CBase64::Encode(request);
    std::string result = Protocol::CJsonProtocol::Process(encoded, service, nullptr);
    nlohmann::json json = DecodeResponse(result);

    REQUIRE(json["status"] == "ok");
    REQUIRE(json["data"]["total_books"] == 0);
    REQUIRE(json["data"]["total_authors"] == 0);
    REQUIRE(json["data"]["indexed_archives"] == 0);
}

TEST_CASE("CJsonProtocol::Process with query action returns books list", "[protocol]")
{
    CTempDir tempDir;
    Config::SAppConfig cfg;
    cfg.database.path = (tempDir.GetPath() / "test.db").string();

    Service::CAppService service(cfg);
    std::string request = R"({"action": "query", "params": {"limit": 10, "offset": 0}})";
    std::string encoded = Utils::CBase64::Encode(request);
    std::string result = Protocol::CJsonProtocol::Process(encoded, service, nullptr);
    nlohmann::json json = DecodeResponse(result);

    REQUIRE(json["status"] == "ok");
    REQUIRE(json["data"]["totalFound"] == 0);
}

TEST_CASE("CJsonProtocol::Process query without limit keeps default page size", "[protocol]")
{
    CTempDir tempDir;
    Config::SAppConfig cfg;
    cfg.database.path = (tempDir.GetPath() / "test_default_limit.db").string();

    {
        Db::CDatabase db(cfg.database.path);
        for (int i = 0; i < 150; ++i)
        {
            Inpx::SBookRecord r;
            r.libId = std::to_string(i + 1);
            r.title = "Book " + std::to_string(i + 1);
            r.language = "ru";
            r.genres = {"sf"};
            r.archiveName = "arch1";
            r.authors.push_back({"Author", std::to_string(i + 1), ""});
            (void)db.InsertBook(r, {});
        }
    }

    Service::CAppService service(cfg);
    std::string request = R"({"action": "query", "params": {}})";
    std::string encoded = Utils::CBase64::Encode(request);
    std::string result = Protocol::CJsonProtocol::Process(encoded, service, nullptr);
    nlohmann::json json = DecodeResponse(result);

    REQUIRE(json["status"] == "ok");
    REQUIRE(json["data"]["totalFound"] == 150);
    REQUIRE(json["data"]["books"].size() == 100);
}

TEST_CASE("CJsonProtocol::Process integer param type coercion", "[protocol]")
{
    CTempDir tempDir;
    Config::SAppConfig cfg;
    cfg.database.path = (tempDir.GetPath() / "test.db").string();

    Service::CAppService service(cfg);
    std::string request = R"({"action": "query", "params": {"limit": "5", "offset": "0"}})";
    std::string encoded = Utils::CBase64::Encode(request);
    std::string result = Protocol::CJsonProtocol::Process(encoded, service, nullptr);
    nlohmann::json json = DecodeResponse(result);

    REQUIRE(json["status"] == "ok");
}

TEST_CASE("CJsonProtocol::Process integer coercion rejects partial numeric strings", "[protocol]")
{
    CTempDir tempDir;
    Config::SAppConfig cfg;
    cfg.database.path = (tempDir.GetPath() / "test_partial_numeric.db").string();

    {
        Db::CDatabase db(cfg.database.path);
        for (int i = 0; i < 3; ++i)
        {
            Inpx::SBookRecord r;
            r.libId = std::to_string(i + 1);
            r.title = "Book " + std::to_string(i + 1);
            r.language = "ru";
            r.genres = {"sf"};
            r.archiveName = "arch1";
            r.authors.push_back({"Author", std::to_string(i + 1), ""});
            (void)db.InsertBook(r, {});
        }
    }

    Service::CAppService service(cfg);
    std::string request = R"({"action":"query","params":{"limit":"2oops","offset":"0"}})";
    std::string encoded = Utils::CBase64::Encode(request);
    std::string result = Protocol::CJsonProtocol::Process(encoded, service, nullptr);
    nlohmann::json json = DecodeResponse(result);

    REQUIRE(json["status"] == "ok");
    REQUIRE(json["data"]["totalFound"] == 3);
    REQUIRE(json["data"]["books"].size() == 3);
}

TEST_CASE("CJsonProtocol::Process integer coercion rejects whitespace-prefixed strings", "[protocol]")
{
    CTempDir tempDir;
    Config::SAppConfig cfg;
    cfg.database.path = (tempDir.GetPath() / "test_whitespace_numeric.db").string();

    {
        Db::CDatabase db(cfg.database.path);
        for (int i = 0; i < 3; ++i)
        {
            Inpx::SBookRecord r;
            r.libId = std::to_string(i + 1);
            r.title = "Book " + std::to_string(i + 1);
            r.language = "ru";
            r.genres = {"sf"};
            r.archiveName = "arch1";
            r.authors.push_back({"Author", std::to_string(i + 1), ""});
            (void)db.InsertBook(r, {});
        }
    }

    Service::CAppService service(cfg);
    std::string request = R"({"action":"query","params":{"limit":" 2","offset":"0"}})";
    std::string encoded = Utils::CBase64::Encode(request);
    std::string result = Protocol::CJsonProtocol::Process(encoded, service, nullptr);
    nlohmann::json json = DecodeResponse(result);

    REQUIRE(json["status"] == "ok");
    REQUIRE(json["data"]["totalFound"] == 3);
    REQUIRE(json["data"]["books"].size() == 3);
}

TEST_CASE("CJsonProtocol::Process query action keeps mixed parameter compatibility", "[protocol]")
{
    CTempDir tempDir;
    Config::SAppConfig cfg;
    cfg.database.path = (tempDir.GetPath() / "test.db").string();

    Service::CAppService service(cfg);
    std::string request =
        R"({"action":"query","params":{"title":"Test","language":"ru","withAnnotation":true,"limit":"3","offset":1,"ratingMin":2}})";
    std::string encoded = Utils::CBase64::Encode(request);
    std::string result = Protocol::CJsonProtocol::Process(encoded, service, nullptr);
    nlohmann::json json = DecodeResponse(result);

    REQUIRE(json["status"] == "ok");
    REQUIRE(json.contains("data"));
    REQUIRE(json["data"].contains("totalFound"));
    REQUIRE(json["data"].contains("books"));
}

TEST_CASE("CJsonProtocol::Process get-book keeps string id compatibility", "[protocol]")
{
    CTempDir tempDir;
    Config::SAppConfig cfg;
    cfg.database.path = (tempDir.GetPath() / "test.db").string();

    Service::CAppService service(cfg);
    std::string request = R"({"action":"get-book","params":{"id":"1"}})";
    std::string encoded = Utils::CBase64::Encode(request);
    std::string result = Protocol::CJsonProtocol::Process(encoded, service, nullptr);
    nlohmann::json json = DecodeResponse(result);

    REQUIRE(json["status"] == "error");
    REQUIRE(json["error"] == "Book not found");
}

TEST_CASE("CJsonProtocol::Process export keeps string id and out compatibility", "[protocol]")
{
    CTempDir tempDir;
    Config::SAppConfig cfg;
    cfg.database.path = (tempDir.GetPath() / "test.db").string();

    Service::CAppService service(cfg);
    std::string request = R"({"action":"export","params":{"id":"1","out":"C:/tmp/out"}})";
    std::string encoded = Utils::CBase64::Encode(request);
    std::string result = Protocol::CJsonProtocol::Process(encoded, service, nullptr);
    nlohmann::json json = DecodeResponse(result);

    REQUIRE(json["status"] == "error");
    REQUIRE(json["error"] == "Book not found");
}
