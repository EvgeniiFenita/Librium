add_library(Log STATIC
    Logger.cpp
)

target_include_directories(Log PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

if (MSVC)
    target_compile_options(Log PUBLIC /Zc:preprocessor)
endif()

