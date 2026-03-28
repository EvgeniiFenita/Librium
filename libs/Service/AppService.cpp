#include "AppService.hpp"
#include "Actions/Actions.hpp"
#include "Log/Logger.hpp"

namespace Librium::Service {

CAppService::CAppService(Config::SAppConfig cfg)
    : m_config(std::move(cfg))
{
    RegisterAction(std::make_unique<CImportAction>());
    RegisterAction(std::make_unique<CUpgradeAction>());
    RegisterAction(std::make_unique<CQueryAction>());
    RegisterAction(std::make_unique<CExportAction>());
    RegisterAction(std::make_unique<CStatsAction>());
    RegisterAction(std::make_unique<CGetBookAction>());
}

CAppService::~CAppService() = default;

void CAppService::RegisterAction(std::unique_ptr<IServiceAction> action)
{
    if (action)
    {
        m_actions[action->GetName()] = std::move(action);
    }
}

void CAppService::Dispatch(const IRequest& req, IResponse& res, const std::shared_ptr<Indexer::IProgressReporter>& reporter)
{
    try
    {
        std::string actionName = req.GetAction();
        if (actionName.empty())
        {
            res.SetError("Missing 'action' field");
            return;
        }

        auto it = m_actions.find(actionName);
        LOG_INFO("Executing action: {}", actionName);

        if (it == m_actions.end())
        {
            res.SetError("Unknown action: " + actionName);
            return;
        }

        it->second->Execute(*this, req, res, reporter);
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("Unhandled exception in action '{}': {}", req.GetAction(), e.what());
        res.SetError(e.what());
    }
}

CLibraryApi& CAppService::GetApi()
{
    if (!m_api)
    {
        m_api = std::make_unique<CLibraryApi>(m_config);
    }
    return *m_api;
}

} // namespace Librium::Service
