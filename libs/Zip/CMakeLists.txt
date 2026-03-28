add_library(Zip STATIC
    ZipReader.cpp
)

target_include_directories(Zip PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(Zip PUBLIC
    libzip::zip
    Log
)

