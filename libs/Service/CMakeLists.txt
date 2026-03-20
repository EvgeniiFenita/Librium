add_library(Service STATIC
    AppService.cpp
    Actions/Actions.cpp
    LibraryApi.cpp
    StdIoChannel.cpp
)

target_include_directories(Service PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(Service PUBLIC 
    Database 
    Config 
    QueryLib 
    Indexer
    Log 
    Inpx 
    Zip 
    Fb2
    Utils
    nlohmann_json::nlohmann_json
)
