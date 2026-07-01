add_library(lslreader OBJECT
    LSLReader.cpp
)

target_include_directories(lslreader PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include/lslreader
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include/data_structures
)

target_link_libraries(lslreader PUBLIC
    lsl
    concurrentqueue
    config
)
