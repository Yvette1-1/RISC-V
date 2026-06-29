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
    add_executable(riscv_smoke_test
        tests/test_smoke.cpp
    )
    target_link_libraries(riscv_smoke_test PRIVATE riscv_core)
    add_test(NAME riscv_smoke_test COMMAND riscv_smoke_test)
endif()
