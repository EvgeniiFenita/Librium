add_library(Fb2 STATIC
    Fb2Parser.cpp
)

target_include_directories(Fb2 PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(Fb2 PUBLIC
    pugixml::pugixml
    Log
    Utils
)

