include(GoogleTest)

add_library(test_main OBJECT utils/TestMain.cpp)
target_link_libraries(test_main PUBLIC gtest runtime_core)

add_subdirectory(unit_tests)
add_subdirectory(component_tests)
add_subdirectory(cli_tests)