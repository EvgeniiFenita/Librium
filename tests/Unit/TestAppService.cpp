#include <catch2/catch_test_macros.hpp>
#include "Service/AppService.hpp"
#include "Service/Request.hpp"
#include "Service/Response.hpp"
#include "Service/ServiceAction.hpp"
#include "Config/AppConfig.hpp"
#include "Database/QueryTypes.hpp"
#include "Database/Database.hpp"
#include "TestUtils.hpp"
#include <algorithm>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>

using namespace Librium;
using namespace Librium::Tests;

namespace {

class CMockRequest : public Service::IRequest
{
public:
    std::string action;

    std::string GetAction() const override { return action; }
    std::string GetString(const std::string&, const std::string& def) const override { return def; }
    int64_t GetInt(const std::string&, int64_t def) const override { return def; }
    bool GetBool(const std::string&, bool def) const override { return def; }
    bool HasParam(const std::string&) const override { return false; }
};

class CMockResponse : public Service::IResponse
{
public:
    std::string errorMessage;
    bool errorCalled{false};
    bool dataCalled{false};
    int64_t lastTotalBooks{0};

    void SetError(const std::string& msg) override
    {
        errorMessage = msg;
        errorCalled = true;
    }

    void SetData(const Db::SImportStats&) override { dataCalled = true; }
    void SetData(const Db::SQueryResult&) override { dataCalled = true; }

    void SetData(const Service::SAppStats& stats) override
    {
        dataCalled = true;
        lastTotalBooks = stats.totalBooks;
    }

    void SetData(const Service::SBookDetails&) override { dataCalled = true; }
    void SetDataExport(const std::filesystem::path&, const std::string&) override { dataCalled = true; }
    void SetProgress(size_t, size_t) override {}
};

class CThrowingAction : public Service::IServiceAction
{
public:
    std::string GetName() const override { return "throw-test"; }
    void Execute(Service::CAppService&, const Service::IRequest&,
                 Service::IResponse&, const std::shared_ptr<Indexer::IProgressReporter>&) override
    {
        throw std::runtime_error("deliberate test exception");
    }
};

class CSuccessAction : public Service::IServiceAction
{
public:
    std::string GetName() const override { return "success-test"; }
    void Execute(Service::CAppService&, const Service::IRequest&,
                 Service::IResponse& res, const std::shared_ptr<Indexer::IProgressReporter>&) override
    {
        Service::SAppStats stats;
        stats.totalBooks = 42;
        res.SetData(stats);
    }
};

class CSuccessAction2 : public Service::IServiceAction
{
public:
    std::string GetName() const override { return "success-test"; }
    void Execute(Service::CAppService&, const Service::IRequest&,
                 Service::IResponse& res, const std::shared_ptr<Indexer::IProgressReporter>&) override
    {
        Service::SAppStats stats;
        stats.totalBooks = 99;
        res.SetData(stats);
    }
};

} // anonymous namespace

TEST_CASE("CAppService::Dispatch with empty action calls SetError", "[service]")
{
    CTempDir tempDir;
    Config::SAppConfig cfg;
    cfg.database.path = (tempDir.GetPath() / "test.db").string();

    Service::CAppService service(cfg);
    CMockRequest req;
    req.action = "";
    CMockResponse res;

    service.Dispatch(req, res, nullptr);

    REQUIRE(res.errorCalled == true);
    std::string lowerErr = res.errorMessage;
    std::transform(lowerErr.begin(), lowerErr.end(), lowerErr.begin(), ::tolower);
    REQUIRE((lowerErr.find("action") != std::string::npos || lowerErr.find("missing") != std::string::npos));
}

TEST_CASE("CAppService::Dispatch with unknown action calls SetError", "[service]")
{
    CTempDir tempDir;
    Config::SAppConfig cfg;
    cfg.database.path = (tempDir.GetPath() / "test.db").string();

    Service::CAppService service(cfg);
    CMockRequest req;
    req.action = "nonexistent_xyz";
    CMockResponse res;

    service.Dispatch(req, res, nullptr);

    REQUIRE(res.errorCalled == true);
    REQUIRE((res.errorMessage.find("nonexistent_xyz") != std::string::npos || res.errorMessage.find("Unknown") != std::string::npos));
}

TEST_CASE("CAppService::Dispatch handles action exception via SetError", "[service]")
{
    CTempDir tempDir;
    Config::SAppConfig cfg;
    cfg.database.path = (tempDir.GetPath() / "test.db").string();

    Service::CAppService service(cfg);
    service.RegisterAction(std::make_unique<CThrowingAction>());

    CMockRequest req;
    req.action = "throw-test";
    CMockResponse res;

    service.Dispatch(req, res, nullptr);

    REQUIRE(res.errorCalled == true);
    REQUIRE(res.errorMessage == "deliberate test exception");
}

TEST_CASE("CAppService::RegisterAction replaces existing action with same name", "[service]")
{
    CTempDir tempDir;
    Config::SAppConfig cfg;
    cfg.database.path = (tempDir.GetPath() / "test.db").string();

    Service::CAppService service(cfg);
    service.RegisterAction(std::make_unique<CSuccessAction>());
    service.RegisterAction(std::make_unique<CSuccessAction2>());

    CMockRequest req;
    req.action = "success-test";
    CMockResponse res;

    service.Dispatch(req, res, nullptr);

    REQUIRE(res.dataCalled == true);
    REQUIRE(res.lastTotalBooks == 99);
}

TEST_CASE("CAppService::GetApi returns same instance on multiple calls", "[service]")
{
    CTempDir tempDir;
    Config::SAppConfig cfg;
    cfg.database.path = (tempDir.GetPath() / "test.db").string();

    Service::CAppService service(cfg);
    auto& api1 = service.GetApi();
    auto& api2 = service.GetApi();

    REQUIRE(&api1 == &api2);
}

TEST_CASE("CAppService::Dispatch stats action returns data", "[service]")
{
    CTempDir tempDir;
    Config::SAppConfig cfg;
    cfg.database.path = (tempDir.GetPath() / "test.db").string();

    Service::CAppService service(cfg);
    CMockRequest req;
    req.action = "stats";
    CMockResponse res;

    service.Dispatch(req, res, nullptr);

    REQUIRE(res.dataCalled == true);
    REQUIRE(res.errorCalled == false);
}
