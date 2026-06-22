file(GLOB UNIT_TEST_SOURCES "*.cpp")

if(UNIT_TEST_SOURCES)
    add_executable(unit_tests ${UNIT_TEST_SOURCES})
    target_link_libraries(unit_tests PRIVATE gtest_main runtime_core)
    target_include_directories(unit_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)

    gtest_discover_tests(unit_tests PROPERTIES LABELS "unit")
endif()