add_library(Database STATIC
    DatabaseArchive.cpp
    Database.cpp
    DatabaseImport.cpp
    DatabaseMaintenance.cpp
    DatabaseQuery.cpp
    DatabaseStats.cpp
    DatabaseSchema.cpp
    SearchQueryParser.cpp
    SqliteDatabase.cpp
    SqliteFunctions.cpp
    SqliteStatement.cpp
)

target_include_directories(Database PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(Database PUBLIC
    Fb2
    Inpx
    Sqlite3Lib
    Log
)
