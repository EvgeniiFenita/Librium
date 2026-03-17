add_executable(Librium
    Main.cpp
    Indexer/Indexer.cpp
    Commands/CIndexCommand.cpp
    Commands/CQueryCommand.cpp
    Commands/CHelperCommands.cpp
)

target_include_directories(Librium PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(Librium PRIVATE
    Config
    Database
    Log
    QueryLib
    CLI11::CLI11
)

if(WIN32)
    target_link_libraries(Librium PRIVATE shell32)
endif()
