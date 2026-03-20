#include <catch2/catch_session.hpp>
#include "Log/Logger.hpp"

int main(int argc, char* argv[]) 
{
    // Initialize logging for unit tests: Debug level for detailed traces.
    Librium::Log::CLogger::Setup(Librium::Log::ELogLevel::Debug);

    int result = Catch::Session().run(argc, argv);

    return result;
}
