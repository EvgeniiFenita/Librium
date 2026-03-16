cmake_minimum_required(VERSION 3.25)
project(libindexer_project VERSION 0.1.0 LANGUAGES CXX C)

# Suppress warnings from third-party dependencies fetched via FetchContent
set(CMAKE_WARN_DEPRECATED OFF CACHE BOOL "" FORCE)
set(CMAKE_SUPPRESS_DEVELOPER_WARNINGS ON CACHE BOOL "" FORCE)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)

# FetchContent is only kept for SQLite3 as we need to build it from amalgamation
include(FetchContent)

find_package(nlohmann_json CONFIG REQUIRED)
find_package(pugixml CONFIG REQUIRED)
find_package(libzip CONFIG REQUIRED)
find_package(CLI11 CONFIG REQUIRED)
find_package(Catch2 CONFIG REQUIRED)

FetchContent_Declare(sqlite3_src
    URL https://www.sqlite.org/2024/sqlite-amalgamation-3450200.zip
)
FetchContent_MakeAvailable(sqlite3_src)

add_library(sqlite3_lib STATIC
    ${sqlite3_src_SOURCE_DIR}/sqlite3.c
)
target_include_directories(sqlite3_lib PUBLIC ${sqlite3_src_SOURCE_DIR})
target_compile_definitions(sqlite3_lib PRIVATE
    SQLITE_THREADSAFE=2
    SQLITE_DEFAULT_MEMSTATUS=0
    SQLITE_DEFAULT_WAL_SYNCHRONOUS=1
    SQLITE_LIKE_DOESNT_MATCH_BLOBS
    SQLITE_MAX_EXPR_DEPTH=0
    SQLITE_OMIT_DEPRECATED
    SQLITE_OMIT_SHARED_CACHE
    SQLITE_ENABLE_FTS5
    SQLITE_ENABLE_JSON1
)

add_subdirectory(libs/core)
add_subdirectory(apps/indexer)
add_subdirectory(apps/query)

enable_testing()
add_subdirectory(tests)

find_program(POWERSHELL_EXE NAMES pwsh powershell)
if(POWERSHELL_EXE)
    add_custom_target(integration_tests
        COMMAND ${POWERSHELL_EXE}
            -ExecutionPolicy Bypass
            -File "${CMAKE_SOURCE_DIR}/tests/integration/Run-Tests.ps1"
            -Config "$<CONFIG>"
        DEPENDS libindexer libquery
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        COMMENT "Running PowerShell end-to-end integration tests"
        VERBATIM
    )
endif()
