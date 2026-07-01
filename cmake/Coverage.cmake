# cmake/Coverage.cmake

option(NEURON_IDE_ENABLE_COVERAGE "Enable code coverage reporting" OFF)

if(NEURON_IDE_ENABLE_COVERAGE)
    find_program(GCOVR_PATH gcovr)
    
    if(NOT GCOVR_PATH)
        message(FATAL_ERROR "gcovr not found! Aborting configuration...")
    endif()
    
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        message(STATUS "Adding coverage compiler flags")
        add_compile_options(--coverage)
        add_link_options(--coverage)
    else()
        message(FATAL_ERROR "Code coverage is only supported with GCC or Clang")
    endif()

    set(COVERAGE_DIR "${CMAKE_BINARY_DIR}/coverage")
    
    add_custom_target(coverage
        COMMAND ${CMAKE_COMMAND} -E make_directory ${COVERAGE_DIR}
        COMMAND ctest --output-on-failure
        COMMAND ${GCOVR_PATH}
            --root ${CMAKE_SOURCE_DIR}
            --exclude ".*_deps.*"
            --exclude ".*tests.*"
            --exclude ".*test.*"
            --exclude ".*protoFiles.*"
            --exclude ".*pb.*"
            --exclude ".*\\.hpp"
            --exclude-throw-branches
            --exclude-unreachable-branches
            --fail-under-line 90
            --print-summary
            --html-details ${COVERAGE_DIR}/index.html
            --xml ${COVERAGE_DIR}/coverage.xml
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Running tests and generating code coverage report..."
    )
endif()
