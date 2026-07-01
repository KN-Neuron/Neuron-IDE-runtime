add_library(config OBJECT
    ConfigParser.cpp
)

target_include_directories(config PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include/config
)

target_link_libraries(config PRIVATE nlohmann_json::nlohmann_json)
