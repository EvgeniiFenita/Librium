#include "Version.hpp"
#include "ProtocolProgressReporter.hpp"
#include "Service/AppService.hpp"
#include "Service/StdIoChannel.hpp"
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
        CLogger::Setup(CLogger::ParseLevel(cfg.logging.level, ELogLevel::Info), cfg.logging.file.empty() ? "Librium.log" : cfg.logging.file);

        LOG_INFO("Librium Engine v{} started with config: {}", VersionString, configPath);

        CAppService engine(std::move(cfg));
        CProtocolProgressReporter reporter;

        CStdIoChannel channel;
        engine.Run(channel, &reporter);

        LOG_INFO("Librium Engine stopped");
    }
    catch (const std::exception& e)
    {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
