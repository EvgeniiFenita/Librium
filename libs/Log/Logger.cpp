#include "Logger.hpp"

#include <chrono>
#include <ctime>
#include <iostream>

namespace Librium::Log {

CLogger& CLogger::Instance() 
{
    static CLogger instance;
    return instance;
}

void CLogger::SetLevel(ELogLevel level) 
{
    std::lock_guard lock(m_mutex);
    m_level = level;
}

void CLogger::SetFile(const std::string& path) 
{
    std::lock_guard lock(m_mutex);
    if (m_file.is_open())
        m_file.close();
    if (!path.empty())
        m_file.open(path, std::ios::app);
}

void CLogger::Log(ELogLevel level, const std::string& message) 
{
    if (level < m_level)
        return;

    // Build timestamp
    auto now = std::chrono::system_clock::now();
    auto t   = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    char ts[16];
    std::strftime(ts, sizeof(ts), "%H:%M:%S", &tm);

    const char* levelStr = "INFO";
    switch (level) 
{
        case ELogLevel::Debug: levelStr = "DEBUG"; break;
        case ELogLevel::Info:  levelStr = "INFO";  break;
        case ELogLevel::Warn:  levelStr = "WARN";  break;
        case ELogLevel::Error: levelStr = "ERROR"; break;
    }

    const std::string line = std::string("[") + ts + "] [" + levelStr + "] " + message + "\n";

    std::lock_guard lock(m_mutex);
    if (m_file.is_open()) 
{
        m_file << line;
        m_file.flush();
    }
    else
    {
        std::cout << line;
        std::cout.flush();
    }
}

} // namespace Librium::Log






