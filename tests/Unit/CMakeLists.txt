find_package(Python3 COMPONENTS Interpreter REQUIRED)

add_executable(UnitTests
    TestMain.cpp
    TestVersion.cpp
    TestZip.cpp
    TestZipInstance.cpp
    TestInpx.cpp
    TestInpxExtra.cpp
    TestFb2.cpp
    TestDb.cpp
    TestDbTransactions.cpp
    TestDatabaseQuery.cpp
    TestQueryEdgeCases.cpp
    TestConfig.cpp
    TestBookFilter.cpp
    TestConfigUtils.cpp
    TestLogger.cpp
    TestLoggerExtra.cpp
    TestIndexer.cpp
    TestStringUtils.cpp
    TestBase64.cpp
    TestThreadSafeQueue.cpp
    TestSearchQueryParser.cpp
    TestSqliteFunctions.cpp
)

target_include_directories(UnitTests PRIVATE
    ${CMAKE_SOURCE_DIR}/apps/Librium
)

target_link_libraries(UnitTests PRIVATE
    Config
    Database
    Log
    Indexer
    Utils
    Catch2::Catch2
)

target_compile_definitions(UnitTests PRIVATE
    # Defined for backwards compatibility if needed, but tests now manage their own temp dirs
    LIBRIUM_TEST_DATA_DIR="${CMAKE_CURRENT_BINARY_DIR}"
)

include(CTest)
include(Catch)
catch_discover_tests(UnitTests)
