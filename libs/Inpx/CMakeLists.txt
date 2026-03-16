add_library(Inpx STATIC
    InpParser.cpp
)

target_include_directories(Inpx PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(Inpx PUBLIC
    Zip
)

