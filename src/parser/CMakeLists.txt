add_library(parser OBJECT
    Parser.cpp
)

target_include_directories(parser PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../include/parser)

target_link_libraries(parser PUBLIC scene)