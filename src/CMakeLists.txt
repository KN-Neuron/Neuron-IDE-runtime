protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS neuronide.proto)

add_library(runtime_core 
    Runtime.cpp
    Parser.cpp 
    ${PROTO_SRCS} 
    ${PROTO_HDRS}
)
target_include_directories(runtime_core SYSTEM PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_BINARY_DIR}                 # Tutaj CMake wrzuca wygenerowane pliki scene.pb.h
)

target_link_libraries(runtime_core PUBLIC protobuf::libprotobuf)

add_executable(NeuronIDE main.cpp)
target_link_libraries(NeuronIDE PRIVATE runtime_core)