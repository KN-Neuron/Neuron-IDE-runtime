add_library(scene OBJECT
    SceneObject.cpp
    Scene.cpp
    components/ComponentRegistry.cpp
    components/BlinkComponent.cpp
)
target_include_directories(scene PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
target_link_libraries(scene PUBLIC neuronide_proto)