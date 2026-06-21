cmake_minimum_required(VERSION 3.20)

project(fanta CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# Set C++ Standard to 23
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------------------------------------------------
# Libraries
# ---------------------------------------------------------

# VM runtime library (CPU & memory mechanics)
add_library(vm_lib STATIC vm/cpu.cpp)
target_include_directories(vm_lib PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/vm
    ${CMAKE_CURRENT_SOURCE_DIR}/common
)

# Compiler core library
add_library(compiler_lib STATIC 
    compiler/codegen.cpp 
    compiler/SimpleIRPass.cpp 
    compiler/ir.cpp 
    compiler/allocator.cpp 
    compiler/instruction_emit.cpp
)
target_include_directories(compiler_lib PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/compiler
    ${CMAKE_CURRENT_SOURCE_DIR}/common
)

# Interface library for tests
add_library(test_framework INTERFACE)
target_include_directories(test_framework INTERFACE 
    ${CMAKE_CURRENT_SOURCE_DIR}/common
    ${CMAKE_CURRENT_SOURCE_DIR}/compiler
    ${CMAKE_CURRENT_SOURCE_DIR}/vm
)

# ---------------------------------------------------------
# Test Executables
# ---------------------------------------------------------

add_executable(ops_test test/ops.cpp)
add_executable(cpu_test test/cpu.cpp)
add_executable(string_assembler test/string_assembler.cpp)
add_executable(lexer_test test/lexer.cpp)
add_executable(parser_test test/parser.cpp)
add_executable(codegen_test test/codegen.cpp)
add_executable(simpleirpass_test test/simpleirpass.cpp)

# Link targets
target_link_libraries(ops_test PRIVATE vm_lib test_framework)
target_link_libraries(cpu_test PRIVATE vm_lib test_framework)
target_link_libraries(string_assembler PRIVATE vm_lib test_framework)
target_link_libraries(lexer_test PRIVATE compiler_lib test_framework)
target_link_libraries(parser_test PRIVATE compiler_lib test_framework)
target_link_libraries(simpleirpass_test PRIVATE compiler_lib test_framework)
target_link_libraries(codegen_test PRIVATE compiler_lib vm_lib test_framework)

# Enable testing
enable_testing()
add_test(NAME ops_test COMMAND ops_test)
add_test(NAME cpu_test COMMAND cpu_test)
add_test(NAME string_assembler COMMAND string_assembler)
add_test(NAME lexer_test COMMAND lexer_test)
add_test(NAME parser_test COMMAND parser_test)
add_test(NAME codegen_test COMMAND codegen_test)
add_test(NAME simpleirpass_test COMMAND simpleirpass_test)

# ---------------------------------------------------------
# Main Executables
# ---------------------------------------------------------

# The Fanta Console (VM Runtime)
find_package(SDL2 REQUIRED)
add_executable(fanta vm/main.cpp)
target_link_libraries(fanta PRIVATE vm_lib SDL2::SDL2)

# The Fanta Compiler
add_executable(fanta-compiler compiler/compiler_main.cpp)
target_link_libraries(fanta-compiler PRIVATE compiler_lib)

# ---------------------------------------------------------
# Modal TUI & Debug Utils
# ---------------------------------------------------------

find_package(Curses REQUIRED)

# The Interactive Debugger TUI
add_executable(fanta-tui tui/main.cpp tui/tui.cpp)
target_include_directories(fanta-tui PRIVATE tui vm compiler common)
target_link_libraries(fanta-tui PRIVATE vm_lib compiler_lib ${CURSES_LIBRARIES})

# TUI Tests
add_executable(tui-tests tui/tests.cpp tui/tui.cpp)
target_include_directories(tui-tests PRIVATE tui vm compiler common)
target_link_libraries(tui-tests PRIVATE vm_lib compiler_lib ${CURSES_LIBRARIES})

# Trace Utility
add_executable(fanta-trace tui/trace.cpp)
target_include_directories(fanta-trace PRIVATE tui vm compiler common)
target_link_libraries(fanta-trace PRIVATE vm_lib)

# Diff Utility
add_executable(fanta-diff tui/diff.cpp)
target_include_directories(fanta-diff PRIVATE tui vm compiler common)
target_link_libraries(fanta-diff PRIVATE vm_lib)
