cmake_minimum_required(VERSION 3.20)

project(fanta CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# Set C++ Standard to 23
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Create a static library for the core logic
add_library(fanta_lib STATIC src/cpu.cpp src/codegen.cpp src/SimpleIRPass.cpp)
target_include_directories(fanta_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Create an interface library for the test framework
add_library(test_framework INTERFACE)
target_include_directories(test_framework INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Test executable
add_executable(ops_test test/ops.cpp)
add_executable(cpu_test test/cpu.cpp)
add_executable(string_assembler test/string_assembler.cpp)
add_executable(lexer_test test/lexer.cpp)
add_executable(parser_test test/parser.cpp)
add_executable(codegen_test test/codegen.cpp)
add_executable(simpleirpass_test test/simpleirpass.cpp)

# Link the library
# We manually add the src directory as SYSTEM to satisfy the requirement
# that headers in src/ are available as system headers (suppressing warnings).
target_include_directories(ops_test SYSTEM PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(ops_test PRIVATE fanta_lib test_framework)

target_include_directories(cpu_test SYSTEM PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(cpu_test PRIVATE fanta_lib test_framework)

target_include_directories(string_assembler SYSTEM PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(string_assembler PRIVATE fanta_lib test_framework)

target_include_directories(lexer_test SYSTEM PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(lexer_test PRIVATE fanta_lib test_framework)

target_include_directories(parser_test SYSTEM PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(parser_test PRIVATE fanta_lib test_framework)

target_include_directories(codegen_test SYSTEM PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(codegen_test PRIVATE fanta_lib test_framework)

target_include_directories(simpleirpass_test SYSTEM PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(simpleirpass_test PRIVATE fanta_lib test_framework)

# Enable testing
enable_testing()
add_test(NAME ops_test COMMAND ops_test)
add_test(NAME cpu_test COMMAND cpu_test)
add_test(NAME string_assembler COMMAND string_assembler)
add_test(NAME lexer_test COMMAND lexer_test)
add_test(NAME parser_test COMMAND parser_test)
add_test(NAME codegen_test COMMAND codegen_test)
add_test(NAME simpleirpass_test COMMAND simpleirpass_test)

# Main Application
find_package(SDL2 REQUIRED)
add_executable(fanta src/main.cpp)
target_link_libraries(fanta PRIVATE fanta_lib SDL2::SDL2)

# TUI Application
find_package(Curses REQUIRED)
add_executable(fanta-tui tui/main.cpp tui/tui.cpp)
target_include_directories(fanta-tui PRIVATE tui src)
target_link_libraries(fanta-tui PRIVATE fanta_lib ${CURSES_LIBRARIES})

# TUI Tests
add_executable(tui-tests tui/tests.cpp tui/tui.cpp)
target_include_directories(tui-tests PRIVATE tui src)
target_link_libraries(tui-tests PRIVATE fanta_lib ${CURSES_LIBRARIES})

# Trace Utility
add_executable(fanta-trace tui/trace.cpp)
target_include_directories(fanta-trace PRIVATE tui src)
target_link_libraries(fanta-trace PRIVATE fanta_lib)

# Diff Utility
add_executable(fanta-diff tui/diff.cpp)
target_include_directories(fanta-diff PRIVATE tui src)
target_link_libraries(fanta-diff PRIVATE fanta_lib)
