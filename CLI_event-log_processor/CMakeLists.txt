cmake_minimum_required(VERSION 3.14)
project(logproc LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

# nlohmann/json 
FetchContent_Declare(
  json
  URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
)
FetchContent_MakeAvailable(json)

# GoogleTest 
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
# Windows only: avoid CRT mismatch errors
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Your code: library + executable 
add_library(logproc_lib src/parser.cpp src/event_processor.cpp)
target_include_directories(logproc_lib PUBLIC src)
target_link_libraries(logproc_lib PUBLIC nlohmann_json::nlohmann_json)
if(NOT MSVC)
  target_compile_options(logproc_lib PUBLIC -Wall -Wextra -Wpedantic)
endif()

add_executable(logproc src/main.cpp)
target_link_libraries(logproc PRIVATE logproc_lib)

# Tests 
enable_testing()
add_executable(tests tests/test_parser.cpp tests/test_processor.cpp)
target_link_libraries(tests PRIVATE logproc_lib GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(tests)