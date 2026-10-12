# A separate executable on purpose: it references no component class, so it sees exactly the
# components that linking runtime_core brings in through REGISTER_COMPONENT, as NeuronIDE does.
add_executable(registration_tests registration_test.cpp)
target_link_libraries(registration_tests PRIVATE gtest_main runtime_core)
target_include_directories(registration_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)

gtest_discover_tests(registration_tests PROPERTIES LABELS "component")
