add_library(Database STATIC
    Database.cpp
    DatabaseSchema.cpp
)

target_include_directories(Database PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(Database PUBLIC
    Fb2
    Inpx
    sqlite3_lib
    Log
)

