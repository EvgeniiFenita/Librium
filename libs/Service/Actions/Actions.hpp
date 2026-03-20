#pragma once

#include "Service/IServiceAction.hpp"
#include <nlohmann/json.hpp>

namespace Librium::Service {

class CImportAction : public IServiceAction
{
public:
    std::string GetName() const override { return "import"; }
    nlohmann::json Execute(CAppService& service, const nlohmann::json& params, Indexer::IProgressReporter* reporter = nullptr) override;
};

class CUpgradeAction : public IServiceAction
{
public:
    std::string GetName() const override { return "upgrade"; }
    nlohmann::json Execute(CAppService& service, const nlohmann::json& params, Indexer::IProgressReporter* reporter = nullptr) override;
};

class CQueryAction : public IServiceAction
{
public:
    std::string GetName() const override { return "query"; }
    nlohmann::json Execute(CAppService& service, const nlohmann::json& params, Indexer::IProgressReporter* reporter = nullptr) override;
};

class CExportAction : public IServiceAction
{
public:
    std::string GetName() const override { return "export"; }
    nlohmann::json Execute(CAppService& service, const nlohmann::json& params, Indexer::IProgressReporter* reporter = nullptr) override;
};

class CStatsAction : public IServiceAction
{
public:
    std::string GetName() const override { return "stats"; }
    nlohmann::json Execute(CAppService& service, const nlohmann::json& params, Indexer::IProgressReporter* reporter = nullptr) override;
};

class CGetBookAction : public IServiceAction
{
public:
    std::string GetName() const override { return "get-book"; }
    nlohmann::json Execute(CAppService& service, const nlohmann::json& params, Indexer::IProgressReporter* reporter = nullptr) override;
};

} // namespace Librium::Service
