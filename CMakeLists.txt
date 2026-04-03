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

find_package(nlohmann_json QUIET)
if(NOT TARGET nlohmann_json::nlohmann_json)
    find_path(NLOHMANN_JSON_INCLUDE_DIR nlohmann/json.hpp REQUIRED)
    add_library(nlohmann_json::nlohmann_json INTERFACE IMPORTED)
    target_include_directories(nlohmann_json::nlohmann_json INTERFACE
        "${NLOHMANN_JSON_INCLUDE_DIR}"
    )
endif()

find_package(HDF5 QUIET COMPONENTS C)
if(HDF5_FOUND)
    message(STATUS "HDF5 support enabled")
else()
    message(STATUS "HDF5 support disabled (library not found)")
endif()

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
    src/lib/configuration/simulator_config.cpp
    src/lib/numerics/state_advancement.cpp
    src/lib/output/run_output.cpp
    src/lib/orchestrator/cli.cpp
    src/lib/orchestrator/simulation_run.cpp
)

target_include_directories(project_lib
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
target_link_libraries(project_lib PUBLIC nlohmann_json::nlohmann_json)
if(HDF5_FOUND)
    if(TARGET HDF5::HDF5)
        target_link_libraries(project_lib PRIVATE HDF5::HDF5)
    else()
        target_include_directories(project_lib PRIVATE ${HDF5_INCLUDE_DIRS})
        target_link_libraries(project_lib PRIVATE ${HDF5_LIBRARIES})
    endif()
    target_compile_definitions(project_lib PUBLIC PROJECT_HAS_HDF5=1)
else()
    target_compile_definitions(project_lib PUBLIC PROJECT_HAS_HDF5=0)
endif()
target_compile_definitions(project_lib PUBLIC PROJECT_VERSION_STRING="${PROJECT_VERSION}")

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

    add_executable(project_tests_unit
        tests/unit/test_state_advancement.cpp
        tests/unit/test_simulator_config.cpp
        tests/unit/test_simulator_config_mechanics.cpp
        tests/unit/test_simulation_run.cpp
        tests/unit/test_simulation_run_advancer.cpp
        tests/unit/test_run_outputs.cpp
    )
    target_link_libraries(project_tests_unit PRIVATE project_lib GTest::gtest GTest::gtest_main Threads::Threads)
    set_project_warnings(project_tests_unit)
    enable_sanitizers(project_tests_unit)

    add_executable(project_tests_integration
        tests/integration/test_simulator_config_integration.cpp
        tests/integration/test_orchestrator_integration.cpp
        tests/integration/test_run_outputs_integration.cpp
    )
    target_link_libraries(project_tests_integration PRIVATE project_lib GTest::gtest GTest::gtest_main Threads::Threads)
    set_project_warnings(project_tests_integration)
    enable_sanitizers(project_tests_integration)

    add_executable(project_tests_system
        tests/system/test_simulator_config_system.cpp
        tests/system/test_headless_simulation_system.cpp
        tests/system/test_headless_outputs_system.cpp
    )
    target_link_libraries(project_tests_system PRIVATE project_lib GTest::gtest GTest::gtest_main Threads::Threads)
    set_project_warnings(project_tests_system)
    enable_sanitizers(project_tests_system)
    target_compile_definitions(project_tests_system PRIVATE PROJECT_APP_PATH="$<TARGET_FILE:project_app>")

    add_executable(project_tests_aux_headers
        tests/aux/header_self_containment_main.cpp
        tests/aux/include_cli.cpp
        tests/aux/include_geometry.cpp
        tests/aux/include_mechanics_state.cpp
        tests/aux/include_run_output.cpp
        tests/aux/include_run_result.cpp
        tests/aux/include_state_advancement.cpp
        tests/aux/include_simulation_run.cpp
        tests/aux/include_simulator_config.cpp
    )
    target_link_libraries(project_tests_aux_headers PRIVATE project_lib)
    set_project_warnings(project_tests_aux_headers)
    enable_sanitizers(project_tests_aux_headers)

    include(GoogleTest)
    gtest_add_tests(TARGET project_tests_unit TEST_LIST unit_tests_list)
    gtest_add_tests(TARGET project_tests_integration TEST_LIST integration_tests_list)
    gtest_add_tests(TARGET project_tests_system TEST_LIST system_tests_list)
    add_test(NAME project_tests_aux_headers COMMAND project_tests_aux_headers)

    set_tests_properties(
        ${unit_tests_list}
        PROPERTIES
            LABELS "unit"
            TIMEOUT 30
            ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0;LSAN_OPTIONS=detect_leaks=0"
    )
    set_tests_properties(
        ${integration_tests_list}
        PROPERTIES
            LABELS "integration"
            TIMEOUT 45
            ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0;LSAN_OPTIONS=detect_leaks=0"
    )
    set_tests_properties(
        ${system_tests_list}
        PROPERTIES
            LABELS "system"
            TIMEOUT 60
            ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0;LSAN_OPTIONS=detect_leaks=0"
    )
    set_tests_properties(
        project_tests_aux_headers
        PROPERTIES
            LABELS "aux"
            TIMEOUT 15
            ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0;LSAN_OPTIONS=detect_leaks=0"
    )
endif()
