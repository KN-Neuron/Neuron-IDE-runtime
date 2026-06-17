protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${CMAKE_CURRENT_SOURCE_DIR}/../protoFiles/neuronide.proto)

add_library(neuronide_proto STATIC ${PROTO_SRCS} ${PROTO_HDRS})
set_target_properties(neuronide_proto PROPERTIES CXX_CLANG_TIDY "")
target_include_directories(neuronide_proto SYSTEM PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(neuronide_proto PUBLIC protobuf::libprotobuf)

add_subdirectory(scene)
add_subdirectory(parser)
add_subdirectory(renderer)
add_subdirectory(datawriter)

add_library(runtime_core STATIC
    Runtime.cpp
)
target_include_directories(runtime_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../include)
target_link_libraries(runtime_core PUBLIC 
    scene 
    parser 
    renderer 
    datawriter
)

add_executable(NeuronIDE main.cpp)
target_link_libraries(NeuronIDE PRIVATE runtime_core)