#pragma once

#include "Config/AppConfig.hpp"
#include "Service/LibraryApi.hpp"
#include "Service/ServiceAction.hpp"
#include <unordered_map>
#include <memory>
#include <string>

namespace Librium::Indexer { class IProgressReporter; }

namespace Librium::Service {

class CAppService
{
public:
    explicit CAppService(Config::SAppConfig cfg);
    ~CAppService();

    void RegisterAction(std::unique_ptr<IServiceAction> action);
    
    void Dispatch(const IRequest& req, IResponse& res, Indexer::IProgressReporter* reporter = nullptr);

    CLibraryApi& GetApi();

private:
    Config::SAppConfig m_config;
    std::unique_ptr<CLibraryApi> m_api;
    std::unordered_map<std::string, std::unique_ptr<IServiceAction>> m_actions;
};

} // namespace Librium::Service
