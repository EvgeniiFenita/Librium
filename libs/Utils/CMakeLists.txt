add_library(Utils STATIC
    StringUtils.cpp
    Base64.cpp
    GenreTranslator.cpp
)

target_include_directories(Utils PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
