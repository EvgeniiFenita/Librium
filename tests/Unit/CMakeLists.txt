find_package(Python3 COMPONENTS Interpreter REQUIRED)

set(UNIT_TEST_DATA_DIR "${CMAKE_CURRENT_BINARY_DIR}/data")
set(UNIT_TEST_DATA_GENERATED "${UNIT_TEST_DATA_DIR}/test.zip")

file(MAKE_DIRECTORY "${UNIT_TEST_DATA_DIR}")

add_custom_command(
    OUTPUT "${UNIT_TEST_DATA_GENERATED}"
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/CreateTestData.py "${UNIT_TEST_DATA_DIR}"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/CreateTestData.py
    COMMENT "Generating unit test data archives via Python3..."
)

add_custom_target(generate_unit_test_data DEPENDS "${UNIT_TEST_DATA_GENERATED}")

add_executable(UnitTests
    TestVersion.cpp
    TestZip.cpp
    TestInpx.cpp
    TestFb2.cpp
    TestDb.cpp
    TestQuery.cpp
    TestConfig.cpp
    TestLogger.cpp
)

target_include_directories(UnitTests PRIVATE
    ${CMAKE_SOURCE_DIR}/apps/Librium
)

target_link_libraries(UnitTests PRIVATE
    Config
    Database
    Log
    QueryLib
    CLI11::CLI11
    Catch2::Catch2WithMain
)

target_compile_definitions(UnitTests PRIVATE
    LIBRIUM_TEST_DATA_DIR="${UNIT_TEST_DATA_DIR}"
)

add_dependencies(UnitTests generate_unit_test_data)

include(CTest)
include(Catch)
catch_discover_tests(UnitTests)

