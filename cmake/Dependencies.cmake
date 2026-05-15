# cmake/Dependencies.cmake

include(FetchContent)

# 1. Google Test
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# 2. LSL
FetchContent_Declare(
  liblsl
  GIT_REPOSITORY https://github.com/sccn/liblsl.git
  GIT_TAG v1.16.2
)
set(LSL_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(liblsl)

# 3. Moodycamel ConcurrentQueue
FetchContent_Declare(
  concurrentqueue
  GIT_REPOSITORY https://github.com/cameron314/concurrentqueue.git
  GIT_TAG v1.0.4
)
FetchContent_MakeAvailable(concurrentqueue)

# 4. SDL2 (System installed)
find_package(SDL2 REQUIRED)