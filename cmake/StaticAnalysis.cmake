# cmake/StaticAnalysis.cmake

# --- Clang-Tidy Configuration ---
find_program(CLANG_TIDY_EXE NAMES "clang-tidy")
if(CLANG_TIDY_EXE)
    message(STATUS "clang-tidy found: ${CLANG_TIDY_EXE}")
    set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_EXE}")
else()
    message(WARNING "clang-tidy not found.")
endif()

# --- Clang-Format Target ---
find_program(CLANG_FORMAT_EXE NAMES "clang-format")
if(CLANG_FORMAT_EXE)
    file(GLOB_RECURSE ALL_CXX_SOURCE_FILES
        "src/*.cpp" "src/*.hpp"
        "include/*.hpp"
        "tests/*.cpp" "tests/*.hpp"
    )
    add_custom_target(
        format
        COMMAND ${CLANG_FORMAT_EXE} -i -style=file ${ALL_CXX_SOURCE_FILES}
        COMMENT "Formatting source code with clang-format"
    )
else()
    message(WARNING "clang-format not found.")
endif()