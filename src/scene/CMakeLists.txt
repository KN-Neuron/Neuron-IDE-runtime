add_library(scene OBJECT
    SceneObject.cpp
    Scene.cpp
    Image.cpp
    ImageLibrary.cpp
    components/ComponentRegistry.cpp
    components/BlinkComponent.cpp
    components/MarkerEmitterComponent.cpp
    components/SpriteRenderer.cpp
)
target_include_directories(scene PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    ${SDL2_INCLUDE_DIRS}
)
target_link_libraries(scene PUBLIC
    neuronide_proto
    ${SDL2_LIBRARIES}
    SDL2_image::SDL2_image
)

target_link_libraries(scene PRIVATE logging)
