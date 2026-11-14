set(DEFAULT_FONT ${CMAKE_SOURCE_DIR}/assets/fonts/DejaVuSans.ttf)
set(DEFAULT_FONT_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/DefaultFont.cpp)
set(EMBED_DEFAULT_FONT ${CMAKE_SOURCE_DIR}/cmake/EmbedDefaultFont.cmake)

add_custom_command(
    OUTPUT ${DEFAULT_FONT_SOURCE}
    COMMAND ${CMAKE_COMMAND} -DINPUT=${DEFAULT_FONT} -DOUTPUT=${DEFAULT_FONT_SOURCE}
            -P ${EMBED_DEFAULT_FONT}
    DEPENDS ${DEFAULT_FONT} ${EMBED_DEFAULT_FONT}
    COMMENT "Embedding the default font"
    VERBATIM
)
add_library(default_font STATIC ${DEFAULT_FONT_SOURCE})
set_target_properties(default_font PROPERTIES CXX_CLANG_TIDY "")

add_library(scene OBJECT
    SceneObject.cpp
    Scene.cpp
    Bitmap.cpp
    Font.cpp
    Image.cpp
    ImageLibrary.cpp
    components/ComponentRegistry.cpp
    components/BlinkComponent.cpp
    components/MarkerEmitterComponent.cpp
    components/SpriteRenderer.cpp
    components/TextRenderer.cpp
    components/ScriptComponent.cpp
)
target_include_directories(scene PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    ${SDL2_INCLUDE_DIRS}
)
target_link_libraries(scene PUBLIC
    neuronide_proto
    ${SDL2_LIBRARIES}
    SDL2_image::SDL2_image
    SDL2_ttf::SDL2_ttf
)

target_link_libraries(scene PRIVATE logging default_font pybind11::embed)
# pybind11 declares its namespace hidden, and GCC warns about any type of default visibility that
# holds a py::object (ScriptComponent's state). Nothing here is exported from a shared library.
set_target_properties(scene PROPERTIES CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN ON)
