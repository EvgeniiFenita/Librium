#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>
#include "Log/Logger.hpp"

using namespace Librium::Log;

namespace {
std::string ReadFile(const std::string& p) 
{
    std::ifstream f(p);
    return {std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
}
} // namespace

TEST_CASE("Info writes [INFO] tag", "[logger]") 
{
    const std::string tmp = "tlog1.log";
    std::filesystem::remove(tmp);
    CLogger::Instance().SetLevel(ELogLevel::Info);
    CLogger::Instance().SetFile(tmp);
    CLogger::Instance().Info("Hello");
    CLogger::Instance().SetFile("");
    REQUIRE(ReadFile(tmp).find("[INFO]") != std::string::npos);
    std::filesystem::remove(tmp);
}
TEST_CASE("Debug suppressed below Info level", "[logger]") 
{
    const std::string tmp = "tlog2.log";
    std::filesystem::remove(tmp);
    CLogger::Instance().SetLevel(ELogLevel::Info);
    CLogger::Instance().SetFile(tmp);
    CLogger::Instance().Debug("hidden");
    CLogger::Instance().Info("visible");
    CLogger::Instance().SetFile("");
    auto c = ReadFile(tmp);
    REQUIRE(c.find("hidden")  == std::string::npos);
    REQUIRE(c.find("visible") != std::string::npos);
    std::filesystem::remove(tmp);
}
TEST_CASE("Format overload works", "[logger]") 
{
    const std::string tmp = "tlog3.log";
    std::filesystem::remove(tmp);
    CLogger::Instance().SetFile(tmp);
    CLogger::Instance().Info("x={}", 99);
    CLogger::Instance().SetFile("");
    REQUIRE(ReadFile(tmp).find("x=99") != std::string::npos);
    std::filesystem::remove(tmp);
}
TEST_CASE("Thread-safe under concurrent writes", "[logger]") 
{
    const std::string tmp = "tlog4.log";
    std::filesystem::remove(tmp);
    CLogger::Instance().SetFile(tmp);
    
    std::vector<std::thread> threads;
    for (int i = 0; i < 8; ++i)
        threads.emplace_back([i]() 
{ for (int j=0;j<100;++j) CLogger::Instance().Info("t{}j{}",i,j); });
    for (auto& t : threads) t.join();
    
    CLogger::Instance().SetFile("");
    std::filesystem::remove(tmp);
    SUCCEED("No crash");
}






