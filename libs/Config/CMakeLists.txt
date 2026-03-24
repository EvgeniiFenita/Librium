add_library(Config STATIC
    AppConfig.cpp
)

target_include_directories(Config PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(Config PUBLIC
    Inpx
    Utils
    nlohmann_json::nlohmann_json
)

