add_executable(libindexer
    Main.cpp
    Indexer/Indexer.cpp
)

target_include_directories(libindexer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(libindexer PRIVATE
    libindexer_core
    CLI11::CLI11
)
