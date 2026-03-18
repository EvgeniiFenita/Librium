find_package(Python3 COMPONENTS Interpreter REQUIRED)

add_executable(UnitTests
    TestMain.cpp
    TestVersion.cpp
    TestZip.cpp
    TestInpx.cpp
    TestFb2.cpp
    TestDb.cpp
    TestQuery.cpp
    TestConfig.cpp
    TestLogger.cpp
    TestIndexer.cpp
)

target_include_directories(UnitTests PRIVATE
    ${CMAKE_SOURCE_DIR}/apps/Librium
)

target_link_libraries(UnitTests PRIVATE
    Config
    Database
    Log
    QueryLib
    Indexer
    Catch2::Catch2
)

target_compile_definitions(UnitTests PRIVATE
    # Defined for backwards compatibility if needed, but tests now manage their own temp dirs
    LIBRIUM_TEST_DATA_DIR="${CMAKE_CURRENT_BINARY_DIR}"
)

include(CTest)
include(Catch)
catch_discover_tests(UnitTests)
