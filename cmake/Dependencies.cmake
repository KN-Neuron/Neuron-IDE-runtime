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

FetchContent_MakeAvailable(googletest liblsl concurrentqueue)

# Restore compiler flags for our own project code
set(CMAKE_C_FLAGS "${BACKUP_C_FLAGS}")
set(CMAKE_CXX_FLAGS "${BACKUP_CXX_FLAGS}")

# 4. SDL2 (System installed)
find_package(SDL2 REQUIRED)

# 5. Protobuf (System installed)
find_package(Protobuf REQUIRED)
