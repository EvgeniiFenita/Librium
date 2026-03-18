#include "AppService.hpp"
#include "Actions/Actions.hpp"
#include "Log/Logger.hpp"

namespace Librium::Service {

CAppService::CAppService(Config::CAppConfig cfg)
    : m_config(std::move(cfg))
{
    RegisterAction("import",      std::make_unique<CImportAction>());
    RegisterAction("upgrade",     std::make_unique<CUpgradeAction>());
    RegisterAction("query",       std::make_unique<CQueryAction>());
    RegisterAction("export",      std::make_unique<CExportAction>());
    RegisterAction("stats",       std::make_unique<CStatsAction>());
}

CAppService::~CAppService() = default;

void CAppService::RegisterAction(const std::string& name, std::unique_ptr<IServiceAction> action)
{
    m_actions[name] = std::move(action);
}

nlohmann::json CAppService::Dispatch(const nlohmann::json& command, Indexer::IProgressReporter* reporter)
{
    try
    {
        if (!command.contains("action"))
        {
            return {{"status", "error"}, {"error", "Missing 'action' field"}};
        }

        std::string actionName = command["action"];
        auto it = m_actions.find(actionName);
        
        if (it == m_actions.end())
        {
            return {{"status", "error"}, {"error", "Unknown action: " + actionName}};
        }

        nlohmann::json params = command.value("params", nlohmann::json::object());
        return it->second->Execute(*this, params, reporter);
    }
    catch (const std::exception& e)
    {
        return {{"status", "error"}, {"error", e.what()}};
    }
}

Db::CDatabase& CAppService::GetDatabase()
{
    if (!m_db)
    {
        m_db = std::make_unique<Db::CDatabase>(m_config.database.path);
    }
    return *m_db;
}

const Config::CAppConfig& CAppService::GetConfig() const
{
    return m_config;
}

} // namespace Librium::Service
