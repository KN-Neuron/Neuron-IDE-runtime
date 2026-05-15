add_library(runtime_core Runtime.cpp)
target_include_directories(runtime_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../include)

target_link_libraries(runtime_core PUBLIC 
    lsl
    concurrentqueue
    ${SDL2_LIBRARIES}
)
target_include_directories(runtime_core PUBLIC ${SDL2_INCLUDE_DIRS})

add_executable(NeuronIDE main.cpp)
target_link_libraries(NeuronIDE PRIVATE runtime_core)