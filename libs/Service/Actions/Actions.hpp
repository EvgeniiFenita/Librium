#pragma once

#include "Service/IServiceAction.hpp"

namespace Librium::Service {

class CImportAction : public IServiceAction
{
public:
    std::string GetName() const override { return "import"; }
    void Execute(CAppService& service, const IRequest& req, IResponse& res, Indexer::IProgressReporter* reporter = nullptr) override;
};

class CUpgradeAction : public IServiceAction
{
public:
    std::string GetName() const override { return "upgrade"; }
    void Execute(CAppService& service, const IRequest& req, IResponse& res, Indexer::IProgressReporter* reporter = nullptr) override;
};

class CQueryAction : public IServiceAction
{
public:
    std::string GetName() const override { return "query"; }
    void Execute(CAppService& service, const IRequest& req, IResponse& res, Indexer::IProgressReporter* reporter = nullptr) override;
};

class CExportAction : public IServiceAction
{
public:
    std::string GetName() const override { return "export"; }
    void Execute(CAppService& service, const IRequest& req, IResponse& res, Indexer::IProgressReporter* reporter = nullptr) override;
};

class CStatsAction : public IServiceAction
{
public:
    std::string GetName() const override { return "stats"; }
    void Execute(CAppService& service, const IRequest& req, IResponse& res, Indexer::IProgressReporter* reporter = nullptr) override;
};

class CGetBookAction : public IServiceAction
{
public:
    std::string GetName() const override { return "get-book"; }
    void Execute(CAppService& service, const IRequest& req, IResponse& res, Indexer::IProgressReporter* reporter = nullptr) override;
};

} // namespace Librium::Service
