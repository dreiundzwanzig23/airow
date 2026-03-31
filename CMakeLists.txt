cmake_minimum_required(VERSION 3.22)
project(agentic_tetris LANGUAGES CXX VERSION 0.2.0)

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    include(CheckCXXSourceCompiles)
    set(CMAKE_REQUIRED_QUIET ON)
    check_cxx_source_compiles("#include <string>\n#ifdef _LIBCPP_VERSION\nint main(){return 0;}\n#else\n#error not libc++\n#endif" DETECT_LIBCXX)

    if(NOT DETECT_LIBCXX)
        find_program(GCC_EXECUTABLE NAMES gcc)
        if(GCC_EXECUTABLE)
            execute_process(COMMAND ${GCC_EXECUTABLE} -dumpmachine OUTPUT_VARIABLE GCC_TRIPLE OUTPUT_STRIP_TRAILING_WHITESPACE)
            execute_process(COMMAND ${GCC_EXECUTABLE} -dumpversion OUTPUT_VARIABLE GCC_VERSION OUTPUT_STRIP_TRAILING_WHITESPACE)
            set(DEFAULT_GCC_LIBDIR "/usr/lib/gcc/${GCC_TRIPLE}/${GCC_VERSION}")
            if(EXISTS "${DEFAULT_GCC_LIBDIR}/libstdc++.so")
                message(STATUS "Clang + libstdc++: adding default GCC libdir: ${DEFAULT_GCC_LIBDIR}")
                add_link_options("-L${DEFAULT_GCC_LIBDIR}")
            else()
                message(WARNING "Clang + libstdc++: expected libstdc++.so not found at ${DEFAULT_GCC_LIBDIR}.")
            endif()
        endif()
    endif()
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(ENABLE_COVERAGE "Enable LLVM source-based coverage instrumentation" OFF)
if(ENABLE_COVERAGE)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        message(FATAL_ERROR "ENABLE_COVERAGE requires Clang/LLVM tooling")
    endif()
    set(COVERAGE_PROFILE_PATTERN "${CMAKE_BINARY_DIR}/profiles/%p-%m.profraw")
    add_compile_options("-fprofile-instr-generate=${COVERAGE_PROFILE_PATTERN}" -fcoverage-mapping)
    add_link_options("-fprofile-instr-generate=${COVERAGE_PROFILE_PATTERN}" -fcoverage-mapping)
endif()

include(cmake/CompilerWarnings.cmake)
include(cmake/Sanitizers.cmake)

add_library(project_lib
    src/lib/string_utils.cpp
)

target_include_directories(project_lib
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

set_project_warnings(project_lib)
enable_sanitizers(project_lib)

add_executable(project_app src/app/main.cpp)
target_link_libraries(project_app PRIVATE project_lib)
set_project_warnings(project_app)
enable_sanitizers(project_app)

include(CTest)
if(BUILD_TESTING)
    include(CheckCXXSourceCompiles)
    set(CMAKE_REQUIRED_QUIET ON)
    check_cxx_source_compiles("#include <string>\n#ifndef _LIBCPP_VERSION\n#error not libc++\n#endif\nint main(){return 0;}" USING_LIBCXX)

    if(USING_LIBCXX)
        message(STATUS "Detected libc++: building GoogleTest from /usr/src/googletest (Ubuntu libgtest-dev sources).")
        if(EXISTS "/usr/src/googletest/CMakeLists.txt")
            add_subdirectory(/usr/src/googletest ${CMAKE_BINARY_DIR}/_deps/googletest EXCLUDE_FROM_ALL)
            if(NOT TARGET GTest::gtest)
                add_library(GTest::gtest ALIAS gtest)
            endif()
            if(NOT TARGET GTest::gtest_main)
                add_library(GTest::gtest_main ALIAS gtest_main)
            endif()
        else()
            message(FATAL_ERROR "libgtest-dev not installed or /usr/src/googletest missing. Install with: sudo apt install libgtest-dev")
        endif()
    else()
        find_package(GTest REQUIRED)
        if(NOT TARGET GTest::gtest)
            add_library(GTest::gtest ALIAS GTest::GTest)
        endif()
        if(NOT TARGET GTest::gtest_main)
            add_library(GTest::gtest_main ALIAS GTest::Main)
        endif()
    endif()

    find_package(Threads REQUIRED)

    add_executable(project_tests_unit tests/unit/test_string_utils.cpp)
    target_link_libraries(project_tests_unit PRIVATE project_lib GTest::gtest GTest::gtest_main Threads::Threads)
    set_project_warnings(project_tests_unit)
    enable_sanitizers(project_tests_unit)

    add_executable(project_tests_integration tests/integration/test_string_utils_integration.cpp)
    target_link_libraries(project_tests_integration PRIVATE project_lib GTest::gtest GTest::gtest_main Threads::Threads)
    set_project_warnings(project_tests_integration)
    enable_sanitizers(project_tests_integration)

    add_executable(project_tests_system tests/system/test_string_utils_system.cpp)
    target_link_libraries(project_tests_system PRIVATE project_lib GTest::gtest GTest::gtest_main Threads::Threads)
    set_project_warnings(project_tests_system)
    enable_sanitizers(project_tests_system)

    include(GoogleTest)
    gtest_add_tests(TARGET project_tests_unit TEST_LIST unit_tests_list)
    gtest_add_tests(TARGET project_tests_integration TEST_LIST integration_tests_list)
    gtest_add_tests(TARGET project_tests_system TEST_LIST system_tests_list)

    set_tests_properties(
        ${unit_tests_list}
        PROPERTIES
            LABELS "unit"
            ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0;LSAN_OPTIONS=detect_leaks=0"
    )
    set_tests_properties(
        ${integration_tests_list}
        PROPERTIES
            LABELS "integration"
            ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0;LSAN_OPTIONS=detect_leaks=0"
    )
    set_tests_properties(
        ${system_tests_list}
        PROPERTIES
            LABELS "system"
            ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0;LSAN_OPTIONS=detect_leaks=0"
    )
endif()
