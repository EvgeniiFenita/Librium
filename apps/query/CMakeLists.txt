add_executable(libquery
    Main.cpp
)

target_include_directories(libquery PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(libquery PRIVATE
    libindexer_core
    CLI11::CLI11
)
