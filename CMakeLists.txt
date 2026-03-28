cmake_minimum_required(VERSION 3.25)
project(Librium VERSION 0.2.0 LANGUAGES CXX C)

# Ensure VCPKG_ROOT is set
if(NOT DEFINED ENV{VCPKG_ROOT})
    message(FATAL_ERROR "VCPKG_ROOT environment variable is not set. Please set it to your vcpkg installation directory.")
endif()

# Suppress warnings from third-party dependencies fetched via FetchContent
set(CMAKE_WARN_DEPRECATED OFF CACHE BOOL "" FORCE)
set(CMAKE_SUPPRESS_DEVELOPER_WARNINGS ON CACHE BOOL "" FORCE)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra)
endif()

include(FetchContent)

# FetchContent is only kept for SQLite3 as we need to build it from amalgamation
find_package(nlohmann_json CONFIG REQUIRED)
find_package(pugixml CONFIG REQUIRED)
find_package(libzip CONFIG REQUIRED)
find_package(Catch2 CONFIG REQUIRED)
find_package(asio CONFIG REQUIRED)

FetchContent_Declare(sqlite3_src
    URL https://www.sqlite.org/2024/sqlite-amalgamation-3450200.zip
)
FetchContent_MakeAvailable(sqlite3_src)

add_library(Sqlite3Lib STATIC
    ${sqlite3_src_SOURCE_DIR}/sqlite3.c
)
target_include_directories(Sqlite3Lib PUBLIC ${sqlite3_src_SOURCE_DIR})
target_compile_definitions(Sqlite3Lib PRIVATE
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

# ------------------------------------------------------------------
# Libraries (lib)
# ------------------------------------------------------------------
add_subdirectory(libs/Log)
add_subdirectory(libs/Utils)
add_subdirectory(libs/Zip)
add_subdirectory(libs/Fb2)
add_subdirectory(libs/Inpx)
add_subdirectory(libs/Config)
add_subdirectory(libs/Database)
add_subdirectory(libs/Indexer)
add_subdirectory(libs/Service)
add_subdirectory(libs/Transport)
add_subdirectory(libs/Protocol)

# ------------------------------------------------------------------
# Applications (bin)
# ------------------------------------------------------------------
add_subdirectory(apps/Librium)

# ------------------------------------------------------------------
# Tests (test)
# ------------------------------------------------------------------
enable_testing()
add_subdirectory(tests/Unit)
