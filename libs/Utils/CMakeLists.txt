add_library(Utils STATIC
    StringUtils.cpp
    Base64.cpp
)

target_include_directories(Utils PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)

# Config is needed for SysUtils.hpp (SAppConfig, Utf8ToPath)
# Log is needed for SetupLogging in SysUtils.hpp
target_link_libraries(Utils PUBLIC Config Log)
