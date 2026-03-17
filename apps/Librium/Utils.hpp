#pragma once

#include "Config/AppConfig.hpp"
#include "Log/Logger.hpp"
#include <string>
#include <filesystem>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#endif

namespace Librium::Apps {

inline void SetupLogging(const Log::ELogLevel level, const std::string& file = "") 
{
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    auto& logger = Log::CLogger::Instance();
    logger.ClearOutputs();
    logger.SetLevel(level);
    
    if (!file.empty())
    {
        logger.AddFileOutput(file);
    }
    else
    {
        // Fallback to console only if no log file is specified
        logger.AddConsoleOutput();
    }
}

inline Log::ELogLevel ParseLogLevel(const std::string& lvl, Log::ELogLevel def = Log::ELogLevel::Info) 
{
    if      (lvl == "debug") return Log::ELogLevel::Debug;
    else if (lvl == "warn")  return Log::ELogLevel::Warn;
    else if (lvl == "error") return Log::ELogLevel::Error;
    else if (lvl == "info")  return Log::ELogLevel::Info;
    return def;
}

using Config::Utf8ToPath;

#ifdef _WIN32
inline std::string utf16_to_utf8(const std::wstring& utf16) 
{
    if (utf16.empty()) return {};
    auto u8 = std::filesystem::path(utf16).u8string();
    return std::string(u8.begin(), u8.end());
}

inline std::vector<std::string> get_utf8_args() 
{
    int argc;
    LPWSTR* argvw = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argvw) return {};
    std::vector<std::string> args;
    for (int i = 0; i < argc; ++i) args.push_back(utf16_to_utf8(argvw[i]));
    LocalFree(argvw);
    return args;
}
#endif

} // namespace Librium::Apps
