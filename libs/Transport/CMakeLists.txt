add_library(Transport STATIC
    AsioTcpServer.cpp
)

if(WIN32)
    target_compile_definitions(Transport PRIVATE _WIN32_WINNT=0x0601)
endif()

target_include_directories(Transport PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(Transport PUBLIC 
    Log
    asio::asio
)
