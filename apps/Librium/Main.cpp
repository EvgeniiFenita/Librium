#include "Version.hpp"
#include "Service/AppService.hpp"
#include "Utils/Base64.hpp"
#include "Utils/StringUtils.hpp"
#include "Indexer/IProgressReporter.hpp"
#include "Log/Logger.hpp"

#include <iostream>
#include <string>
#include <vector>
#include <memory>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#endif

using namespace Librium::Apps;
using namespace Librium::Service;
using namespace Librium::Utils;
using namespace Librium::Log;

namespace {

#ifdef _WIN32
std::vector<std::string> GetUtf8Args() 
{
    int argc;
    LPWSTR* argvw = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argvw) return {};
    std::vector<std::string> args;
    for (int i = 0; i < argc; ++i) args.push_back(CStringUtils::Utf16ToUtf8(argvw[i]));
    LocalFree(argvw);
    return args;
}
#endif

class CProtocolProgressReporter : public Librium::Indexer::IProgressReporter
{
public:
    void OnProgress(size_t processed, size_t total) override
    {
        nlohmann::json progress_msg = {
            {"status", "progress"},
            {"data", {
                {"processed", processed},
                {"total", total}
            }}
        };
        std::cout << CBase64::Encode(progress_msg.dump()) << std::endl;
    }
};

void ShowUsage()
{
    std::cout << "Librium Engine v" << VersionString << "\n\n";
    std::cout << "Usage: Librium.exe --config <path_to_config.json>\n\n";
    std::cout << "The engine will start and wait for Base64-encoded JSON commands on stdin.\n";
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

    if (args.size() != 3 || args[1] != "--config")
    {
        ShowUsage();
        return 1;
    }

    std::string configPath = args[2];

    try
    {
        auto cfg = Librium::Config::SAppConfig::Load(configPath);
        
        // Use log file from config if possible, or default
        CLogger::Setup(ELogLevel::Info, cfg.logging.file.empty() ? "Librium.log" : cfg.logging.file);

        LOG_INFO("Librium Engine v{} started with config: {}", VersionString, configPath);

        CAppService engine(std::move(cfg));
        CProtocolProgressReporter reporter;
        std::string line;

        while (std::getline(std::cin, line))
        {
            if (line.empty()) continue;
            if (line == "exit" || line == "quit") break;

            try
            {
                std::string json_str = CBase64::Decode(line);
                LOG_INFO("INCOMING: {}", json_str);

                nlohmann::json command = nlohmann::json::parse(json_str);
                nlohmann::json response = engine.Dispatch(command, &reporter);

                std::string response_str = response.dump();
                LOG_INFO("OUTGOING: {}", response_str);

                std::cout << CBase64::Encode(response_str) << std::endl;
            }
            catch (const std::exception& e)
            {
                LOG_ERROR("Protocol error: {}", e.what());
                
                nlohmann::json err_resp = {
                    {"status", "error"},
                    {"error", std::string("Protocol error: ") + e.what()}
                };
                std::cout << CBase64::Encode(err_resp.dump()) << std::endl;
            }
        }

        LOG_INFO("Librium Engine stopped");
    }
    catch (const std::exception& e)
    {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
