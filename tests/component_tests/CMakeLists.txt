file(GLOB COMP_TEST_SOURCES "*.cpp")

if(COMP_TEST_SOURCES)
    add_executable(component_tests ${COMP_TEST_SOURCES})
    target_link_libraries(component_tests PRIVATE gtest_main runtime_core)

    gtest_discover_tests(component_tests PROPERTIES LABELS "component")
endif()