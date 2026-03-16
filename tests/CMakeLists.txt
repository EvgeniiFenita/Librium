find_package(Python3 COMPONENTS Interpreter REQUIRED)

set(TEST_DATA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/data")
set(TEST_DATA_GENERATED "${TEST_DATA_DIR}/test.zip")

add_custom_command(
    OUTPUT "${TEST_DATA_GENERATED}"
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/CreateTestData.py
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/CreateTestData.py
    COMMENT "Generating test data archives via Python3..."
)

add_custom_target(generate_test_data DEPENDS "${TEST_DATA_GENERATED}")

add_executable(libindexer_tests
    TestVersion.cpp
    TestZip.cpp
    TestInpx.cpp
    TestFb2.cpp
    TestDb.cpp
    TestQuery.cpp
    TestConfig.cpp
    TestLogger.cpp
)

target_include_directories(libindexer_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/apps/indexer
    ${CMAKE_SOURCE_DIR}/apps/query
)

target_link_libraries(libindexer_tests PRIVATE
    libindexer_core
    CLI11::CLI11
    Catch2::Catch2WithMain
)

target_compile_definitions(libindexer_tests PRIVATE
    LIBINDEXER_TEST_DATA_DIR="${TEST_DATA_DIR}"
)

add_dependencies(libindexer_tests generate_test_data)

include(CTest)
include(Catch)
catch_discover_tests(libindexer_tests)
