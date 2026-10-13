# cmake/Dependencies.cmake

include(FetchContent)

# 1. Google Test
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
  SYSTEM
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

# 2. LSL
FetchContent_Declare(
  liblsl
  GIT_REPOSITORY https://github.com/sccn/liblsl.git
  GIT_TAG v1.17.7
  SYSTEM
)
set(LSL_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

# 3. Moodycamel ConcurrentQueue
FetchContent_Declare(
  concurrentqueue
  GIT_REPOSITORY https://github.com/cameron314/concurrentqueue.git
  GIT_TAG v1.0.4
  SYSTEM
)

# 4. nlohmann/json (header-only) - device config parsing
FetchContent_Declare(
  nlohmann_json
  URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
  SYSTEM
)
set(JSON_BuildTests OFF CACHE INTERNAL "")

# 5. Quill (header-only) - asynchronous logging
FetchContent_Declare(
  quill
  URL https://github.com/odygrd/quill/archive/refs/tags/v13.0.0.tar.gz
  URL_HASH SHA256=88b4a1542125577a4d51cf444c51e34d63618c422ba6a4fa9bd23894b49d696b
  SYSTEM
)

# Suppress compiler warnings from third-party targets when compiling their source files
set(BACKUP_C_FLAGS "${CMAKE_C_FLAGS}")
set(BACKUP_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -w")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -w")
elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /w")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /w")
endif()

FetchContent_MakeAvailable(googletest liblsl concurrentqueue nlohmann_json quill)

# Restore compiler flags for our own project code
set(CMAKE_C_FLAGS "${BACKUP_C_FLAGS}")
set(CMAKE_CXX_FLAGS "${BACKUP_CXX_FLAGS}")

# 6. SDL2 (System installed)
find_package(SDL2 REQUIRED)

# 7. Protobuf (System installed)
find_package(Protobuf REQUIRED)
