add_executable(Indexer
    Main.cpp
    Indexer/Indexer.cpp
)

target_include_directories(Indexer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(Indexer PRIVATE
    Config
    Database
    Log
    QueryLib
    CLI11::CLI11
)

