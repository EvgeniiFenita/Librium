add_library(libindexer_core STATIC
    Zip/ZipReader.cpp
    Inpx/InpParser.cpp
    Fb2/Fb2Parser.cpp
    Db/Database.cpp
    Query/BookQuery.cpp
    Query/QuerySerializer.cpp
    Config/AppConfig.cpp
    Log/Logger.cpp
)

target_include_directories(libindexer_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(libindexer_core PUBLIC
    sqlite3_lib
    nlohmann_json::nlohmann_json
    pugixml::pugixml
    libzip::zip
    ${ZLIB_LIBRARIES}
)
