#pragma once

#include "Config/AppConfig.hpp"
#include "LibraryApi.hpp"
#include <nlohmann/json.hpp>
#include <memory>
#include <string>
#include <map>

namespace Librium::Indexer {
    class IProgressReporter;
}

namespace Librium::Service {

class IServiceAction;

class CAppService
{
public:
    explicit CAppService(Config::SAppConfig cfg);
    ~CAppService();

    nlohmann::json Dispatch(const nlohmann::json& command, Indexer::IProgressReporter* reporter = nullptr);

    // Helpers for actions
    [[nodiscard]] CLibraryApi& GetApi();

private:
    void RegisterAction(const std::string& name, std::unique_ptr<IServiceAction> action);

    Config::SAppConfig                                   m_config;
    std::unique_ptr<CLibraryApi>                         m_api;
    std::map<std::string, std::unique_ptr<IServiceAction>> m_actions;
};

} // namespace Librium::Service
