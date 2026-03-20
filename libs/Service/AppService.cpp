#include "AppService.hpp"
#include "Actions/Actions.hpp"
#include "Log/Logger.hpp"
#include "ICommandChannel.hpp"
#include "Utils/Base64.hpp"

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
        LOG_INFO("Executing action: {}", actionName);

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

CLibraryApi& CAppService::GetApi()
{
    if (!m_api)
    {
        m_api = std::make_unique<CLibraryApi>(m_config);
    }
    return *m_api;
}

void CAppService::Run(ICommandChannel& channel, Indexer::IProgressReporter* reporter)
{
    std::string line;
    while (channel.ReadLine(line))
    {
        if (line.empty()) continue;
        if (line == "exit" || line == "quit")
        {
            LOG_DEBUG("INCOMING: {}", line);
            break;
        }

        try
        {
            std::string json_str = Utils::CBase64::Decode(line);
            LOG_DEBUG("INCOMING: {}", json_str);

            nlohmann::json command = nlohmann::json::parse(json_str);
            nlohmann::json response = Dispatch(command, reporter);

            // Use 'replace' strategy for invalid UTF-8 bytes to prevent protocol crashes
            std::string response_str = response.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
            LOG_DEBUG("OUTGOING: {}", response_str);

            channel.WriteLine(Utils::CBase64::Encode(response_str));
        }
        catch (const std::exception& e)
        {
            LOG_ERROR("Protocol error: {}", e.what());
            
            nlohmann::json err_resp = {
                {"status", "error"},
                {"error", std::string("Protocol error: ") + e.what()}
            };
            channel.WriteLine(Utils::CBase64::Encode(err_resp.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace)));
        }
    }
}

} // namespace Librium::Service
