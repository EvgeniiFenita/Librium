add_executable(Librium
    Main.cpp
    Version.hpp
    ProtocolProgressReporter.hpp
)

target_include_directories(Librium PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(Librium PRIVATE 
    Service 
    Transport
    Protocol
    Config 
    Database
    Log
    Utils
)

if(WIN32)
    target_link_libraries(Librium PRIVATE shell32)
endif()
