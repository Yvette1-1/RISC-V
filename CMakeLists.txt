cmake_minimum_required(VERSION 3.20)
project(riscv_simulator_debugger LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_TESTS "Build tests" ON)

add_library(riscv_core
    src/debugger.cpp
    src/elf_loader.cpp
    src/memory.cpp
    src/simulator.cpp
)

target_include_directories(riscv_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_executable(riscv_sim
    src/main.cpp
)

target_link_libraries(riscv_sim PRIVATE riscv_core)

if(BUILD_TESTS)
    enable_testing()

    add_executable(riscv_smoke_test tests/test_smoke.cpp)
    target_link_libraries(riscv_smoke_test PRIVATE riscv_core)
    add_test(NAME riscv_smoke_test COMMAND riscv_smoke_test)

    add_executable(riscv_cpu_test tests/test_cpu.cpp)
    target_link_libraries(riscv_cpu_test PRIVATE riscv_core)
    add_test(NAME riscv_cpu_test COMMAND riscv_cpu_test)

    add_executable(riscv_syscall_test tests/test_syscall.cpp)
    target_link_libraries(riscv_syscall_test PRIVATE riscv_core)
    add_test(NAME riscv_syscall_test COMMAND riscv_syscall_test)

    add_executable(riscv_debugger_test tests/test_debugger.cpp)
    target_link_libraries(riscv_debugger_test PRIVATE riscv_core)
    add_test(NAME riscv_debugger_test COMMAND riscv_debugger_test)

    add_executable(riscv_debugger_edges_test tests/test_debugger_edges.cpp)
    target_link_libraries(riscv_debugger_edges_test PRIVATE riscv_core)
    add_test(NAME riscv_debugger_edges_test COMMAND riscv_debugger_edges_test)

    add_executable(riscv_breakpoint_test tests/test_breakpoint.cpp)
    target_link_libraries(riscv_breakpoint_test PRIVATE riscv_core)
    add_test(NAME riscv_breakpoint_test COMMAND riscv_breakpoint_test)

    add_executable(riscv_memory_test tests/test_memory.cpp)
    target_link_libraries(riscv_memory_test PRIVATE riscv_core)
    add_test(NAME riscv_memory_test COMMAND riscv_memory_test)

    add_executable(riscv_hello_test tests/test_hello.cpp)
    target_link_libraries(riscv_hello_test PRIVATE riscv_core)
    add_test(NAME riscv_hello_test COMMAND riscv_hello_test)

    add_executable(riscv_loop_test tests/test_loop.cpp)
    target_link_libraries(riscv_loop_test PRIVATE riscv_core)
    add_test(NAME riscv_loop_test COMMAND riscv_loop_test)

    add_executable(riscv_recursion_test tests/test_recursion.cpp)
    target_link_libraries(riscv_recursion_test PRIVATE riscv_core)
    add_test(NAME riscv_recursion_test COMMAND riscv_recursion_test)

    add_executable(riscv_array_test tests/test_array.cpp)
    target_link_libraries(riscv_array_test PRIVATE riscv_core)
    add_test(NAME riscv_array_test COMMAND riscv_array_test)

    add_executable(riscv_syscall_edges_test tests/test_syscall_edges.cpp)
    target_link_libraries(riscv_syscall_edges_test PRIVATE riscv_core)
    add_test(NAME riscv_syscall_edges_test COMMAND riscv_syscall_edges_test)

    add_executable(riscv_elf_loader_edges_test tests/test_elf_loader_edges.cpp)
    target_link_libraries(riscv_elf_loader_edges_test PRIVATE riscv_core)
    add_test(NAME riscv_elf_loader_edges_test COMMAND riscv_elf_loader_edges_test)

    add_executable(riscv_illegal_exit_test tests/test_illegal_exit.cpp)
    target_link_libraries(riscv_illegal_exit_test PRIVATE riscv_core)
    add_test(NAME riscv_illegal_exit_test COMMAND riscv_illegal_exit_test)
endif()
