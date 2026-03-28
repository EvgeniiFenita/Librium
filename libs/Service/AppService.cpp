#include "AppService.hpp"
#include "Actions/Actions.hpp"
#include "Log/Logger.hpp"

namespace Librium::Service {

CAppService::CAppService(Config::SAppConfig cfg)
    : m_config(std::move(cfg))
{
    RegisterBuiltInActions();
}

CAppService::~CAppService() = default;

void CAppService::RegisterBuiltInActions()
{
    RegisterAction(std::make_unique<CImportAction>());
    RegisterAction(std::make_unique<CUpgradeAction>());
    RegisterAction(std::make_unique<CQueryAction>());
    RegisterAction(std::make_unique<CExportAction>());
    RegisterAction(std::make_unique<CStatsAction>());
    RegisterAction(std::make_unique<CGetBookAction>());
}

void CAppService::RegisterAction(std::unique_ptr<IServiceAction> action)
{
    if (action)
    {
        m_actions[action->GetName()] = std::move(action);
    }
}

std::optional<std::reference_wrapper<IServiceAction>> CAppService::FindAction(const std::string& actionName) const
{
    const auto it = m_actions.find(actionName);
    if (it == m_actions.end())
    {
        return std::nullopt;
    }

    return std::ref(*it->second);
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

        LOG_INFO("Executing action: {}", actionName);

        auto action = FindAction(actionName);
        if (!action)
        {
            res.SetError("Unknown action: " + actionName);
            return;
        }

        action->get().Execute(*this, req, res, reporter);
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("Unhandled exception in action '{}': {}", req.GetAction(), e.what());
        res.SetError(e.what());
    }
}

CLibraryApi& CAppService::EnsureApi()
{
    if (!m_api)
    {
        m_api = std::make_unique<CLibraryApi>(m_config);
    }

    return *m_api;
}

CLibraryApi& CAppService::GetApi()
{
    return EnsureApi();
}

} // namespace Librium::Service
