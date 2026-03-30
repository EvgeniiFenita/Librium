find_package(Python3 COMPONENTS Interpreter REQUIRED)

add_executable(UnitTests
    TestMain.cpp
    TestVersion.cpp
    TestZip.cpp
    TestZipInstance.cpp
    TestZipEdgeCases.cpp
    TestInpx.cpp
    TestInpxExtra.cpp
    TestInpxEdgeCases.cpp
    TestFb2.cpp
    TestFb2Cover.cpp
    TestFb2Encoding.cpp
    TestDb.cpp
    TestDbTransactions.cpp
    TestDatabaseQuery.cpp
    TestQueryEdgeCases.cpp
    TestDatabaseGetBook.cpp
    TestConfig.cpp
    TestBookFilter.cpp
    TestConfigUtils.cpp
    TestConfigEdgeCases.cpp
    TestLogger.cpp
    TestLoggerExtra.cpp
    TestIndexer.cpp
    TestStringUtils.cpp
    TestStringUtilsSanitize.cpp
    TestBase64.cpp
    TestThreadSafeQueue.cpp
    TestSearchQueryParser.cpp
    TestSearchQueryParserEdgeCases.cpp
    TestSqliteFunctions.cpp
    TestProtocol.cpp
    TestAppService.cpp
    TestLibraryApi.cpp
)

target_include_directories(UnitTests PRIVATE
    ${CMAKE_BINARY_DIR}/apps/Librium
)

target_link_libraries(UnitTests PRIVATE
    Config
    Database
    Log
    Indexer
    Utils
    Protocol
    Service
    Catch2::Catch2
)

target_compile_definitions(UnitTests PRIVATE
    # Defined for backwards compatibility if needed, but tests now manage their own temp dirs
    LIBRIUM_TEST_DATA_DIR="${CMAKE_CURRENT_BINARY_DIR}"
)

if(MSVC)
    target_compile_options(UnitTests PRIVATE /FS)
endif()

include(CTest)

file(GLOB unit_test_discovery_files
    "${CMAKE_CURRENT_BINARY_DIR}/UnitTests-*_include.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/UnitTests-*_tests.cmake"
)
if(unit_test_discovery_files)
    file(REMOVE ${unit_test_discovery_files})
endif()

add_test(
    NAME UnitTests
    COMMAND $<TARGET_FILE:UnitTests> --colour-mode ansi
)
