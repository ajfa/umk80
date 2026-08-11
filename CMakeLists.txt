cmake_minimum_required(VERSION 3.16)
project(umk80 C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wshadow -Wconversion -Wpedantic)
endif()

# --- Core: no dependencies, not even the standard library ------------------
add_library(umk80core STATIC
    core/src/i8080.c
)
target_include_directories(umk80core PUBLIC core/include)

# --- Tests ---------------------------------------------------------------
enable_testing()

add_executable(cpu_suite tests/cpu/cpu_suite.c)
target_link_libraries(cpu_suite PRIVATE umk80core)

add_test(NAME cpu_suites_rapidas
         COMMAND cpu_suite ${CMAKE_CURRENT_SOURCE_DIR}/tests/cpu/suites --quick)

# 8080EXM takes several minutes: a separate test, not part of the quick run.
add_test(NAME cpu_suite_8080exm
         COMMAND cpu_suite ${CMAKE_CURRENT_SOURCE_DIR}/tests/cpu/suites)
set_tests_properties(cpu_suite_8080exm PROPERTIES TIMEOUT 3600 LABELS lenta)
