add_library(QueryLib STATIC
    BookQuery.cpp
    QuerySerializer.cpp
)

target_include_directories(QueryLib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(QueryLib PUBLIC
    Database
    nlohmann_json::nlohmann_json
)
