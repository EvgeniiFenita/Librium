#pragma once

#include "Service/IServiceAction.hpp"
#include <nlohmann/json.hpp>

namespace Librium::Service {

class CImportAction : public IServiceAction
{
public:
    nlohmann::json Execute(CAppService& service, const nlohmann::json& params, Indexer::IProgressReporter* reporter = nullptr) override;
};

class CUpgradeAction : public IServiceAction
{
public:
    nlohmann::json Execute(CAppService& service, const nlohmann::json& params, Indexer::IProgressReporter* reporter = nullptr) override;
};

class CQueryAction : public IServiceAction
{
public:
    nlohmann::json Execute(CAppService& service, const nlohmann::json& params, Indexer::IProgressReporter* reporter = nullptr) override;
};

class CExportAction : public IServiceAction
{
public:
    nlohmann::json Execute(CAppService& service, const nlohmann::json& params, Indexer::IProgressReporter* reporter = nullptr) override;
};

class CStatsAction : public IServiceAction
{
public:
    nlohmann::json Execute(CAppService& service, const nlohmann::json& params, Indexer::IProgressReporter* reporter = nullptr) override;
};

} // namespace Librium::Service
