#include <catch2/catch_test_macros.hpp>

#include "Log/Logger.hpp"
#include "Utils/StringUtils.hpp"
#include "TestUtils.hpp"

#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>
#include <atomic>

using namespace Librium::Log;

namespace {

std::string ReadFileContent(const std::filesystem::path& path)
{
    std::ifstream f(path);
    return {std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
}

} // namespace

TEST_CASE("CLogger::ParseLevel", "[logger]")
{
    SECTION("Parses 'debug'")
    {
        REQUIRE(CLogger::ParseLevel("debug") == ELogLevel::Debug);
    }

    SECTION("Parses 'info'")
    {
        REQUIRE(CLogger::ParseLevel("info") == ELogLevel::Info);
    }

    SECTION("Parses 'warn'")
    {
        REQUIRE(CLogger::ParseLevel("warn") == ELogLevel::Warn);
    }

    SECTION("Parses 'error'")
    {
        REQUIRE(CLogger::ParseLevel("error") == ELogLevel::Error);
    }

    SECTION("Unknown string returns provided default")
    {
        REQUIRE(CLogger::ParseLevel("verbose",  ELogLevel::Warn)  == ELogLevel::Warn);
        REQUIRE(CLogger::ParseLevel("critical", ELogLevel::Debug) == ELogLevel::Debug);
        REQUIRE(CLogger::ParseLevel("",         ELogLevel::Error) == ELogLevel::Error);
    }

    SECTION("Default parameter is Info when not supplied")
    {
        REQUIRE(CLogger::ParseLevel("unknown_level") == ELogLevel::Info);
    }
}

TEST_CASE("CLogger::Setup configures level and file output", "[logger]")
{
    const std::string tmp = "setup_test.log";
    std::filesystem::remove(tmp);

    CLogger::Setup(ELogLevel::Info, tmp);
    LOG_INFO("setup_info_msg");
    LOG_DEBUG("setup_debug_msg"); // should be suppressed at Info level
    CLogger::Instance().ClearOutputs();

    std::string content = ReadFileContent(tmp);
    REQUIRE(content.find("setup_info_msg")  != std::string::npos);
    REQUIRE(content.find("setup_debug_msg") == std::string::npos);

    std::filesystem::remove(tmp);
}

TEST_CASE("CLogger::Setup without file only configures level", "[logger]")
{
    // Setup with no file path should not throw
    REQUIRE_NOTHROW(CLogger::Setup(ELogLevel::Debug));
    CLogger::Instance().ClearOutputs();
}

TEST_CASE("CLogger::AddConsoleOutput does not crash", "[logger]")
{
    CLogger::Instance().ClearOutputs();
    REQUIRE_NOTHROW(CLogger::Instance().AddConsoleOutput());
    LOG_INFO("console output test");
    CLogger::Instance().ClearOutputs();
    SUCCEED("Console output worked without crash");
}

TEST_CASE("CLogger Warn level filtering", "[logger]")
{
    const std::string tmp = "warn_level_test.log";
    std::filesystem::remove(tmp);

    CLogger::Instance().ClearOutputs();
    CLogger::Instance().SetLevel(ELogLevel::Warn);
    CLogger::Instance().AddFileOutput(tmp);

    LOG_DEBUG("debug_hidden");
    LOG_INFO("info_hidden");
    LOG_WARN("warn_visible");
    LOG_ERROR("error_visible");

    CLogger::Instance().ClearOutputs();

    std::string content = ReadFileContent(tmp);
    REQUIRE(content.find("debug_hidden")  == std::string::npos);
    REQUIRE(content.find("info_hidden")   == std::string::npos);
    REQUIRE(content.find("warn_visible")  != std::string::npos);
    REQUIRE(content.find("error_visible") != std::string::npos);
    REQUIRE(content.find("[WARN ]")       != std::string::npos);
    REQUIRE(content.find("[ERROR]")       != std::string::npos);

    std::filesystem::remove(tmp);
}

TEST_CASE("CLogger Error level filtering", "[logger]")
{
    const std::string tmp = "error_level_test.log";
    std::filesystem::remove(tmp);

    CLogger::Instance().ClearOutputs();
    CLogger::Instance().SetLevel(ELogLevel::Error);
    CLogger::Instance().AddFileOutput(tmp);

    LOG_DEBUG("d");
    LOG_INFO("i");
    LOG_WARN("w");
    LOG_ERROR("error_only_visible");

    CLogger::Instance().ClearOutputs();

    std::string content = ReadFileContent(tmp);
    REQUIRE(content.find("error_only_visible") != std::string::npos);
    REQUIRE(content.find("[ERROR]")            != std::string::npos);
    // Nothing else should appear
    REQUIRE(content.find("[DEBUG]") == std::string::npos);
    REQUIRE(content.find("[INFO ]") == std::string::npos);
    REQUIRE(content.find("[WARN ]") == std::string::npos);

    std::filesystem::remove(tmp);
}

TEST_CASE("CLogger Debug level passes all messages", "[logger]")
{
    const std::string tmp = "debug_all_test.log";
    std::filesystem::remove(tmp);

    CLogger::Instance().ClearOutputs();
    CLogger::Instance().SetLevel(ELogLevel::Debug);
    CLogger::Instance().AddFileOutput(tmp);

    LOG_DEBUG("debug_msg");
    LOG_INFO("info_msg");
    LOG_WARN("warn_msg");
    LOG_ERROR("error_msg");

    CLogger::Instance().ClearOutputs();

    std::string content = ReadFileContent(tmp);
    REQUIRE(content.find("debug_msg") != std::string::npos);
    REQUIRE(content.find("info_msg")  != std::string::npos);
    REQUIRE(content.find("warn_msg")  != std::string::npos);
    REQUIRE(content.find("error_msg") != std::string::npos);

    std::filesystem::remove(tmp);
}

TEST_CASE("CLogger concurrent logging from multiple threads", "[logger][concurrent]")
{
    Librium::Tests::CTempDir tempDir;
    const std::string logPath = (tempDir.GetPath() / "concurrent_log_test.log").string();

    CLogger::Instance().ClearOutputs();
    CLogger::Instance().SetLevel(ELogLevel::Debug);
    CLogger::Instance().AddFileOutput(logPath);

    {
        std::vector<std::jthread> threads;
        threads.reserve(4);

        for (int threadId = 0; threadId < 4; ++threadId)
        {
            threads.emplace_back([threadId]()
            {
                for (int msgIndex = 0; msgIndex < 25; ++msgIndex)
                {
                    LOG_INFO("thread{}-msg{}", threadId, msgIndex);
                }
            });
        }
        // std::jthread auto-joins on destruction
    }

    CLogger::Instance().ClearOutputs();

    std::string content = ReadFileContent(logPath);

    int lineCount = 0;
    for (const char c : content)
    {
        if (c == '\n')
            ++lineCount;
    }

    REQUIRE(lineCount >= 100);
}

TEST_CASE("CLogger supports Unicode log file paths", "[logger]")
{
    Librium::Tests::CTempDir tempDir;
    const auto logDir = tempDir.GetPath() / Librium::Utils::CStringUtils::Utf8ToPath("логи");
    std::filesystem::create_directories(logDir);
    const auto logPath = logDir / Librium::Utils::CStringUtils::Utf8ToPath("тест.log");

    CLogger::Instance().ClearOutputs();
    CLogger::Instance().SetLevel(ELogLevel::Info);
    CLogger::Instance().AddFileOutput(Librium::Utils::CStringUtils::PathToUtf8String(logPath));
    LOG_INFO("unicode_log_message");
    CLogger::Instance().ClearOutputs();

    REQUIRE(std::filesystem::exists(logPath));
    std::string content = ReadFileContent(logPath);
    REQUIRE(content.find("unicode_log_message") != std::string::npos);
}
