add_library(renderer OBJECT
    Renderer.cpp
)

target_include_directories(renderer PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include/renderer
    ${SDL2_INCLUDE_DIRS}
)

target_link_libraries(renderer PUBLIC
    scene 
    lsl 
    concurrentqueue 
    ${SDL2_LIBRARIES}
)