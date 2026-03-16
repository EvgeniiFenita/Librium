add_executable(Query
    Main.cpp
)

target_include_directories(Query PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(Query PRIVATE
    Database
    Log
    QueryLib
    CLI11::CLI11
)
