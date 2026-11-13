add_library(scripting OBJECT
    PythonInterpreter.cpp
)
target_include_directories(scripting PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
target_link_libraries(scripting PUBLIC pybind11::embed)
target_link_libraries(scripting PRIVATE logging)
