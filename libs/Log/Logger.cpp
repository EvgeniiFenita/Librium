#include "Logger.hpp"

#include "Utils/StringUtils.hpp"

#include <chrono>
#include <ctime>
#include <cstdio>
#include <filesystem>
#include <thread>
#include <sstream>
#include <iomanip>

namespace Librium::Log {

namespace {

class CFileOutput : public ILogOutput
{
public:
    explicit CFileOutput(const std::string& path)
    {
        m_file.open(Utils::CStringUtils::Utf8ToPath(path), std::ios::app);
        if (!m_file.is_open())
        {
            std::fputs(("WARNING: Cannot open log file: " + path + "\n").c_str(), stderr);
        }
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
        std::fputs(message.c_str(), stdout);
        std::fflush(stdout);
    }
};

[[nodiscard]] std::vector<std::unique_ptr<ILogOutput>> BuildOutputs(const std::string& file)
{
    std::vector<std::unique_ptr<ILogOutput>> outputs;
    if (!file.empty())
    {
        outputs.push_back(std::make_unique<CFileOutput>(file));
    }

    return outputs;
}

[[nodiscard]] std::string_view ToString(ELogLevel level)
{
    switch (level)
    {
        case ELogLevel::Debug: return "DEBUG";
        case ELogLevel::Info:  return "INFO ";
        case ELogLevel::Warn:  return "WARN ";
        case ELogLevel::Error: return "ERROR";
    }

    return "INFO ";
}

[[nodiscard]] ELogLevel ParseLevelString(const std::string& levelStr, ELogLevel defaultLevel)
{
    if (levelStr == "debug") return ELogLevel::Debug;
    if (levelStr == "warn")  return ELogLevel::Warn;
    if (levelStr == "error") return ELogLevel::Error;
    if (levelStr == "info")  return ELogLevel::Info;
    return defaultLevel;
}

[[nodiscard]] std::string BuildTimestamp(const std::chrono::system_clock::time_point& now)
{
    const auto timestamp = std::chrono::system_clock::to_time_t(now);
    const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &timestamp);
#else
    localtime_r(&timestamp, &tm);
#endif

    char buffer[32];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &tm);

    std::stringstream stream;
    stream << buffer << "." << std::setfill('0') << std::setw(3) << milliseconds.count();
    return stream.str();
}

[[nodiscard]] std::string SourceFileName(std::source_location loc)
{
    const auto utf8Name = std::filesystem::path(loc.file_name()).filename().u8string();
    return std::string(utf8Name.begin(), utf8Name.end());
}

[[nodiscard]] std::string BuildLogLine(ELogLevel level, const std::string& message, std::source_location loc)
{
    std::stringstream stream;
    stream << "[" << BuildTimestamp(std::chrono::system_clock::now()) << "] "
           << "[" << std::this_thread::get_id() << "] "
           << "[" << ToString(level) << "] "
           << "[" << SourceFileName(loc) << ":" << loc.line() << "] "
           << message << "\n";
    return stream.str();
}

} // namespace

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
    // Build new outputs before acquiring the lock to minimize critical section duration.
    std::vector<std::unique_ptr<ILogOutput>> newOutputs = BuildOutputs(file);

    auto& logger = CLogger::Instance();
    logger.m_level.store(level, std::memory_order_relaxed);

    // Replace outputs atomically in a single lock — no window where messages are dropped.
    std::lock_guard lock(logger.m_mutex);
    logger.m_outputs = std::move(newOutputs);
}

ELogLevel CLogger::ParseLevel(const std::string& levelStr, ELogLevel defaultLevel)
{
    return ParseLevelString(levelStr, defaultLevel);
}

void CLogger::Log(ELogLevel level, const std::string& message, std::source_location loc)
{
    if (level < m_level.load(std::memory_order_relaxed))
    {
        return;
    }
    const std::string line = BuildLogLine(level, message, loc);

    std::lock_guard lock(m_mutex);
    for (auto& out : m_outputs)
        out->Write(line);
}

} // namespace Librium::Log
