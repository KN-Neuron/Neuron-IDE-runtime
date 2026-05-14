cmake_minimum_required(VERSION 3.14)
project(NeuronIDE CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


enable_testing()

include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)


find_package(Protobuf REQUIRED)
# Kopiuje plik danych do folderu build przy każdej kompilacji
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_scene.pb 
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
     
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(CompilerWarnings)
include(StaticAnalysis)


include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

add_subdirectory(src)
add_subdirectory(tests)