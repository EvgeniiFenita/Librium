#include <catch2/catch_session.hpp>
#include "Log/Logger.hpp"

int main(int argc, char* argv[]) 
{
    // Initialize logging for unit tests: Error level only, or Warn
    // This prevents INFO logs from cluttering the test output.
    Librium::Log::CLogger::Setup(Librium::Log::ELogLevel::Warn);

    int result = Catch::Session().run(argc, argv);

    return result;
}
