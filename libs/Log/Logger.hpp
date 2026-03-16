#pragma once

#include <format>
#include <fstream>
#include <mutex>
#include <string>

namespace Librium::Log {

enum class ELogLevel
{ Debug, Info, Warn, Error };

class CLogger
{
public:
    [[nodiscard]] static CLogger& Instance();

    void SetLevel(ELogLevel level);
    void SetFile(const std::string& path);
    void Log(ELogLevel level, const std::string& message);

    void Debug(const std::string& msg) 
{ Log(ELogLevel::Debug, msg); }
    void Info(const std::string& msg) 
{ Log(ELogLevel::Info,  msg); }
    void Warn(const std::string& msg) 
{ Log(ELogLevel::Warn,  msg); }
    void Error(const std::string& msg) 
{ Log(ELogLevel::Error, msg); }

    template<typename... Args>
    void Debug(std::format_string<Args...> fmt, Args&&... args) 
{ Log(ELogLevel::Debug, std::format(fmt, std::forward<Args>(args)...)); }

    template<typename... Args>
    void Info(std::format_string<Args...> fmt, Args&&... args) 
{ Log(ELogLevel::Info, std::format(fmt, std::forward<Args>(args)...)); }

    template<typename... Args>
    void Warn(std::format_string<Args...> fmt, Args&&... args) 
{ Log(ELogLevel::Warn, std::format(fmt, std::forward<Args>(args)...)); }

    template<typename... Args>
    void Error(std::format_string<Args...> fmt, Args&&... args) 
{ Log(ELogLevel::Error, std::format(fmt, std::forward<Args>(args)...)); }

private:
    CLogger() = default;

    std::mutex    m_mutex;
    ELogLevel     m_level{ELogLevel::Info};
    std::ofstream m_file;
};

} // namespace Librium::Log






