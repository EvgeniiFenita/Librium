#pragma once

#include <format>
#include <fstream>
#include <mutex>
#include <string>
#include <vector>
#include <memory>
#include <source_location>

namespace Librium::Log {

enum class ELogLevel
{ Debug = 0, Info = 1, Warn = 2, Error = 3 };

class ILogOutput
{
public:
    virtual ~ILogOutput() = default;
    virtual void Write(const std::string& message) = 0;
};

class CLogger
{
public:
    [[nodiscard]] static CLogger& Instance();

    void SetLevel(ELogLevel level);
    void AddFileOutput(const std::string& path);
    void AddConsoleOutput();
    void ClearOutputs();

    void Log(ELogLevel level, const std::string& message, 
             std::source_location loc = std::source_location::current());

    template<typename... Args>
    void LogFmt(ELogLevel level, std::source_location loc, std::format_string<Args...> fmt, Args&&... args)
    {
        Log(level, std::format(fmt, std::forward<Args>(args)...), loc);
    }

private:
    CLogger() = default;

    std::mutex m_mutex;
    ELogLevel  m_level{ELogLevel::Info};
    std::vector<std::unique_ptr<ILogOutput>> m_outputs;
};

} // namespace Librium::Log

#define LOG_DEBUG(fmt, ...) ::Librium::Log::CLogger::Instance().LogFmt(::Librium::Log::ELogLevel::Debug, std::source_location::current(), fmt __VA_OPT__(,) __VA_ARGS__)
#define LOG_INFO(fmt, ...)  ::Librium::Log::CLogger::Instance().LogFmt(::Librium::Log::ELogLevel::Info,  std::source_location::current(), fmt __VA_OPT__(,) __VA_ARGS__)
#define LOG_WARN(fmt, ...)  ::Librium::Log::CLogger::Instance().LogFmt(::Librium::Log::ELogLevel::Warn,  std::source_location::current(), fmt __VA_OPT__(,) __VA_ARGS__)
#define LOG_ERROR(fmt, ...) ::Librium::Log::CLogger::Instance().LogFmt(::Librium::Log::ELogLevel::Error, std::source_location::current(), fmt __VA_OPT__(,) __VA_ARGS__)

// For simple strings without formatting
#define LOG_DEBUG_S(msg) ::Librium::Log::CLogger::Instance().Log(::Librium::Log::ELogLevel::Debug, msg, std::source_location::current())
#define LOG_INFO_S(msg)  ::Librium::Log::CLogger::Instance().Log(::Librium::Log::ELogLevel::Info,  msg, std::source_location::current())
#define LOG_WARN_S(msg)  ::Librium::Log::CLogger::Instance().Log(::Librium::Log::ELogLevel::Warn,  msg, std::source_location::current())
#define LOG_ERROR_S(msg) ::Librium::Log::CLogger::Instance().Log(::Librium::Log::ELogLevel::Error, msg, std::source_location::current())






