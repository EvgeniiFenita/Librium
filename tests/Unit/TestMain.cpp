#include <catch2/catch_session.hpp>
#include "Log/Logger.hpp"

int main(int argc, char* argv[]) 
{
    // Redirect all log output to a file so test runs stay clean.
    Librium::Log::CLogger::Setup(Librium::Log::ELogLevel::Debug, "unit_tests.log");

    int result = Catch::Session().run(argc, argv);

    return result;
}
