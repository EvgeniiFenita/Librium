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
#include <limits>
#include <system_error>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#endif

namespace {

struct SCommandLineOptions
{
    std::string configPath;
    uint16_t port{0};
};

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
        args.push_back(Librium::Utils::CStringUtils::Utf16ToUtf8(argvw.get()[i]));
    }
    return args;
}
#endif

bool TryParsePort(std::string_view value, uint16_t& port)
{
    unsigned int parsedPort = 0;
    const auto [ptr, error] = std::from_chars(value.data(), value.data() + value.size(), parsedPort);
    if (error != std::errc{} || ptr != value.data() + value.size() || parsedPort == 0 ||
        parsedPort > std::numeric_limits<uint16_t>::max())
    {
        return false;
    }

    port = static_cast<uint16_t>(parsedPort);
    return true;
}

bool ParseCommandLine(const std::vector<std::string>& args, SCommandLineOptions& options)
{
    for (size_t i = 1; i < args.size(); ++i)
    {
        if (args[i] == "--config" && i + 1 < args.size())
        {
            options.configPath = args[++i];
        }
        else if (args[i] == "--port" && i + 1 < args.size())
        {
            if (!TryParsePort(args[++i], options.port))
            {
                LOG_ERROR("Invalid port number: {}", args[i]);
                return false;
            }
        }
    }

    return !options.configPath.empty() && options.port != 0;
}

void ShowUsage()
{
    std::cout << "Librium Engine v" << Librium::Apps::VersionString << "\n\n";
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

    SCommandLineOptions options;
    if (!ParseCommandLine(args, options))
    {
        ShowUsage();
        return 1;
    }

    try
    {
        auto cfg = Librium::Config::SAppConfig::Load(options.configPath);
        
        // Use log file from config if possible, or default
        Librium::Log::CLogger::Setup(
            Librium::Log::CLogger::ParseLevel(cfg.logging.level, Librium::Log::ELogLevel::Info),
            cfg.logging.file.empty() ? "Librium.log" : cfg.logging.file);

        LOG_INFO("Librium Engine v{} started with config: {}, port: {}", Librium::Apps::VersionString, options.configPath, options.port);

        // engine must live as long as the server
        auto engine = std::make_shared<Librium::Service::CAppService>(std::move(cfg));

        Librium::Transport::CAsioTcpServer::MessageHandlerFactory factory = [engine](Librium::Transport::CAsioTcpServer::SendCallback sendCallback) {
            return [engine, sendCallback](const std::string& request) {
                // Factory that creates a reporter bound to the current response
                Librium::Protocol::CJsonProtocol::ReporterFactory reporterFactory = [](Librium::Service::IResponse& res) {
                    return std::make_shared<Librium::Apps::CProtocolProgressReporter>(res);
                };
                
                // Process the request using the JSON protocol handler
                std::string response = Librium::Protocol::CJsonProtocol::Process(request, *engine, sendCallback, reporterFactory);
                
                // Send the final response
                sendCallback(response);
            };
        };

        Librium::Transport::CAsioTcpServer server(options.port, factory);
        server.Run();

        LOG_INFO("Librium Engine stopped");
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("Fatal error: {}", e.what());
        return 1;
    }

    return 0;
}
