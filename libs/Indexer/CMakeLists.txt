add_library(Indexer STATIC
    IndexerImportSupport.cpp
    Indexer.cpp
    IndexerRun.cpp
    IndexerRunSupport.cpp
    IndexerWorker.cpp
    IndexerWriter.cpp
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
