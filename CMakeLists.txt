cmake_minimum_required(VERSION 3.20)

project(fanta CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Set C++ Standard to 23
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Create a static library for the core logic
add_library(fanta_lib STATIC src/cpu.cpp)
target_include_directories(fanta_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Create an interface library for the test framework
add_library(test_framework INTERFACE)
target_include_directories(test_framework INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Test executable
add_executable(ops_test test/ops.cpp)

# Link the library
# We manually add the src directory as SYSTEM to satisfy the requirement
# that headers in src/ are available as system headers (suppressing warnings).
target_include_directories(ops_test SYSTEM PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(ops_test PRIVATE fanta_lib test_framework)

# Enable testing
enable_testing()
add_test(NAME ops_test COMMAND ops_test)

# Main Application
find_package(SDL2 REQUIRED)
add_executable(fanta src/main.cpp)
target_link_libraries(fanta PRIVATE fanta_lib SDL2::SDL2)
