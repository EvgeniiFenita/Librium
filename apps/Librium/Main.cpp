#include "Version.hpp"
#include "ProtocolProgressReporter.hpp"
#include "Service/AppService.hpp"
#include "Protocol/JsonProtocol.hpp"
#include "Transport/AsioTcpServer.hpp"
#include "Utils/StringUtils.hpp"
#include "Indexer/ProgressReporter.hpp"
#include "Log/Logger.hpp"

#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <charconv>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#endif

using namespace Librium::Apps;
using namespace Librium::Service;
using namespace Librium::Protocol;
using namespace Librium::Transport;
using namespace Librium::Utils;
using namespace Librium::Log;

namespace {

#ifdef _WIN32
std::vector<std::string> GetUtf8Args()
{
    int argc;
    LPWSTR* rawArgvw = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!rawArgvw)
    {
        return {};
    }

    std::unique_ptr<LPWSTR, decltype(&LocalFree)> argvw(rawArgvw, &LocalFree);
    std::vector<std::string> args;
    args.reserve(argc);
    for (int i = 0; i < argc; ++i)
    {
        args.push_back(CStringUtils::Utf16ToUtf8(argvw.get()[i]));
    }
    return args;
}
#endif

void ShowUsage()
{
    std::cout << "Librium Engine v" << VersionString << "\n\n";
    std::cout << "Usage: Librium.exe --config <path_to_config.json> --port <port_number>\n\n";
    std::cout << "The engine will start a TCP server on 127.0.0.1:<port> and wait for a single connection.\n";
}

} // namespace

int main(int argc, char* argv[]) 
{
    (void)argc;
    (void)argv;
    std::vector<std::string> args;
#ifdef _WIN32
    args = GetUtf8Args();
#else
    for (int i = 0; i < argc; ++i) args.push_back(argv[i]);
#endif

    std::string configPath;
    uint16_t port = 0;

    for (size_t i = 1; i < args.size(); ++i)
    {
        if (args[i] == "--config" && i + 1 < args.size())
        {
            configPath = args[++i];
        }
        else if (args[i] == "--port" && i + 1 < args.size())
        {
            try {
                port = static_cast<uint16_t>(std::stoi(args[++i]));
            } catch (const std::exception& e) {
                LOG_ERROR("Invalid port number: {}", e.what());
                return 1;
            }
        }
    }

    if (configPath.empty() || port == 0)
    {
        ShowUsage();
        return 1;
    }

    try
    {
        auto cfg = Librium::Config::SAppConfig::Load(configPath);
        
        // Use log file from config if possible, or default
        CLogger::Setup(CLogger::ParseLevel(cfg.logging.level, ELogLevel::Info), cfg.logging.file.empty() ? "Librium.log" : cfg.logging.file);

        LOG_INFO("Librium Engine v{} started with config: {}, port: {}", VersionString, configPath, port);

        // engine must live as long as the server
        auto engine = std::make_shared<CAppService>(std::move(cfg));

        CAsioTcpServer::MessageHandlerFactory factory = [engine](CAsioTcpServer::SendCallback sendCallback) {
            return [engine, sendCallback](const std::string& request) {
                // Factory that creates a reporter bound to the current response
                CJsonProtocol::ReporterFactory reporterFactory = [](Librium::Service::IResponse& res) {
                    return std::make_unique<CProtocolProgressReporter>(res);
                };
                
                // Process the request using the JSON protocol handler
                std::string response = CJsonProtocol::Process(request, *engine, sendCallback, reporterFactory);
                
                // Send the final response
                sendCallback(response);
            };
        };

        CAsioTcpServer server(port, factory);
        server.Run();

        LOG_INFO("Librium Engine stopped");
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("Fatal error: : {}", e.what());
        return 1;
    }

    return 0;
}
