#include "Logger.hpp"

#include <chrono>
#include <ctime>
#include <iostream>
#include <filesystem>
#include <thread>
#include <sstream>
#include <iomanip>

namespace Librium::Log {

class CFileOutput : public ILogOutput
{
public:
    explicit CFileOutput(const std::string& path)
    {
        m_file.open(path, std::ios::app);
    }

    void Write(const std::string& message) override
    {
        if (m_file.is_open())
        {
            m_file << message;
            m_file.flush();
        }
    }

private:
    std::ofstream m_file;
};

class CConsoleOutput : public ILogOutput
{
public:
    void Write(const std::string& message) override
    {
        std::cout << message;
        std::cout.flush();
    }
};

CLogger& CLogger::Instance() 
{
    static CLogger instance;
    return instance;
}

void CLogger::SetLevel(ELogLevel level)
{
    m_level.store(level, std::memory_order_relaxed);
}
void CLogger::AddFileOutput(const std::string& path)
{
    std::lock_guard lock(m_mutex);
    m_outputs.push_back(std::make_unique<CFileOutput>(path));
}

void CLogger::AddConsoleOutput()
{
    std::lock_guard lock(m_mutex);
    m_outputs.push_back(std::make_unique<CConsoleOutput>());
}

void CLogger::ClearOutputs()
{
    std::lock_guard lock(m_mutex);
    m_outputs.clear();
}

void CLogger::Setup(ELogLevel level, const std::string& file)
{
    auto& logger = CLogger::Instance();
    logger.ClearOutputs();
    logger.SetLevel(level);
    
    if (!file.empty())
    {
        logger.AddFileOutput(file);
    }
    else
    {
        logger.AddConsoleOutput();
    }
}

ELogLevel CLogger::ParseLevel(const std::string& levelStr, ELogLevel defaultLevel)
{
    if      (levelStr == "debug") return ELogLevel::Debug;
    else if (levelStr == "warn")  return ELogLevel::Warn;
    else if (levelStr == "error") return ELogLevel::Error;
    else if (levelStr == "info")  return ELogLevel::Info;
    return defaultLevel;
}

void CLogger::Log(ELogLevel level, const std::string& message, std::source_location loc)
{
    if (level < m_level.load(std::memory_order_relaxed))
        return;
    auto now = std::chrono::system_clock::now();
    auto t   = std::chrono::system_clock::to_time_t(now);
    auto ms  = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif

    char ts[32];
    std::strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tm);

    const char* levelStr = "INFO ";
    switch (level) 
    {
        case ELogLevel::Debug: levelStr = "DEBUG"; break;
        case ELogLevel::Info:  levelStr = "INFO "; break;
        case ELogLevel::Warn:  levelStr = "WARN "; break;
        case ELogLevel::Error: levelStr = "ERROR"; break;
    }

    auto u8name = std::filesystem::path(loc.file_name()).filename().u8string();
    std::string fileName(u8name.begin(), u8name.end());
    
    std::stringstream ss;
    ss << "[" << ts << "." << std::setfill('0') << std::setw(3) << ms.count() << "] "
       << "[" << std::this_thread::get_id() << "] "
       << "[" << levelStr << "] "
       << "[" << fileName << ":" << loc.line() << "] "
       << message << "\n";

    const std::string line = ss.str();

    std::lock_guard lock(m_mutex);
    for (auto& out : m_outputs)
        out->Write(line);
}

} // namespace Librium::Log
