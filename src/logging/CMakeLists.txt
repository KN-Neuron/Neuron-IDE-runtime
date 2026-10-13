add_library(logging OBJECT
    Logging.cpp
)

target_include_directories(logging PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../include)

target_link_libraries(logging PUBLIC quill::quill)
