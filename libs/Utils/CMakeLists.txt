add_library(Utils INTERFACE)

target_include_directories(Utils INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/..)

# Config is needed for SysUtils.hpp (SAppConfig, Utf8ToPath)
# Log is needed for SetupLogging in SysUtils.hpp
target_link_libraries(Utils INTERFACE Config Log)
