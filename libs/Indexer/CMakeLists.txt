add_library(Indexer STATIC
    IndexerArchive.cpp
    IndexerImportSupport.cpp
    Indexer.cpp
)

target_include_directories(Indexer PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(Indexer PUBLIC 
    Database 
    Config 
    Log 
    Inpx 
    Zip 
    Fb2
)
