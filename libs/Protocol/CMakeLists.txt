add_library(Protocol STATIC
    JsonProtocol.cpp
)

target_include_directories(Protocol PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(Protocol PUBLIC 
    Service
    Utils
    nlohmann_json::nlohmann_json
)
