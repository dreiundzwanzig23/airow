cmake_minimum_required(VERSION 3.22)
project(agentic_tetris LANGUAGES C CXX VERSION 0.2.0)

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

set(PROJECT_USING_LIBCXX OFF)
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND DETECT_LIBCXX)
    set(PROJECT_USING_LIBCXX ON)
endif()

set(PROJECT_SANITIZER_TEST_ENV "ASAN_OPTIONS=detect_leaks=0;LSAN_OPTIONS=detect_leaks=0")
if(PROJECT_USING_LIBCXX)
    set(PROJECT_SANITIZER_TEST_ENV
        "ASAN_OPTIONS=detect_leaks=0:alloc_dealloc_mismatch=0;LSAN_OPTIONS=detect_leaks=0"
    )
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

find_path(SUNDIALS_INCLUDE_DIR sundials/sundials_config.h REQUIRED)
find_library(SUNDIALS_GENERIC_LIBRARY NAMES sundials_generic REQUIRED)
find_library(SUNDIALS_NVECSERIAL_LIBRARY NAMES sundials_nvecserial REQUIRED)
find_library(SUNDIALS_SUNMATRIXDENSE_LIBRARY
    NAMES sundials_sunmatrixdense
    REQUIRED
)
find_library(SUNDIALS_SUNLINSOLDENSE_LIBRARY
    NAMES sundials_sunlinsoldense
    REQUIRED
)
find_library(SUNDIALS_IDA_LIBRARY NAMES sundials_ida REQUIRED)
message(STATUS "SUNDIALS support enabled")

find_package(Chrono QUIET CONFIG)
set(PROJECT_CHRONO_ENABLED OFF)
if(Chrono_FOUND)
    if(TARGET Chrono::Chrono_core)
        set(PROJECT_CHRONO_ENABLED ON)
        set(PROJECT_CHRONO_TARGET Chrono::Chrono_core)
    elseif(TARGET Chrono::Chrono)
        set(PROJECT_CHRONO_ENABLED ON)
        set(PROJECT_CHRONO_TARGET Chrono::Chrono)
    elseif(TARGET Chrono::ChronoEngine)
        set(PROJECT_CHRONO_ENABLED ON)
        set(PROJECT_CHRONO_TARGET Chrono::ChronoEngine)
    elseif(DEFINED CHRONO_LIBRARIES)
        set(PROJECT_CHRONO_ENABLED ON)
    endif()
endif()
if(PROJECT_CHRONO_ENABLED AND PROJECT_USING_LIBCXX)
    set(PROJECT_CHRONO_ENABLED OFF)
    unset(PROJECT_CHRONO_TARGET)
    set(PROJECT_CHRONO_DISABLE_REASON
        "installed package is ABI-incompatible with libc++ lanes")
endif()
if(PROJECT_CHRONO_ENABLED)
    message(STATUS "Chrono support enabled")
else()
    if(DEFINED PROJECT_CHRONO_DISABLE_REASON)
        message(STATUS "Chrono support disabled (${PROJECT_CHRONO_DISABLE_REASON})")
    else()
        message(STATUS "Chrono support disabled (library not found)")
    endif()
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
    src/lib/aero/baseline_providers.cpp
    src/lib/configuration/provider_catalog.cpp
    src/lib/configuration/simulator_config.cpp
    src/lib/hydro/baseline_providers.cpp
    src/lib/numerics/backend_catalog.cpp
    src/lib/numerics/state_advancement.cpp
    src/lib/output/run_analysis.cpp
    src/lib/output/run_output.cpp
    src/lib/orchestrator/cli.cpp
    src/lib/orchestrator/scenario_harness.cpp
    src/lib/orchestrator/simulation_run.cpp
)

target_include_directories(project_lib
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
target_include_directories(project_lib PRIVATE "${SUNDIALS_INCLUDE_DIR}")
target_link_libraries(project_lib PUBLIC nlohmann_json::nlohmann_json)
target_link_libraries(project_lib PRIVATE
    ${SUNDIALS_GENERIC_LIBRARY}
    ${SUNDIALS_NVECSERIAL_LIBRARY}
    ${SUNDIALS_SUNMATRIXDENSE_LIBRARY}
    ${SUNDIALS_SUNLINSOLDENSE_LIBRARY}
    ${SUNDIALS_IDA_LIBRARY}
)
target_compile_definitions(project_lib PUBLIC PROJECT_HAS_SUNDIALS=1)
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
if(PROJECT_CHRONO_ENABLED)
    if(DEFINED PROJECT_CHRONO_TARGET)
        target_link_libraries(project_lib PRIVATE ${PROJECT_CHRONO_TARGET})
    else()
        if(DEFINED CHRONO_INCLUDE_DIRS)
            target_include_directories(project_lib PRIVATE ${CHRONO_INCLUDE_DIRS})
        endif()
        target_link_libraries(project_lib PRIVATE ${CHRONO_LIBRARIES})
    endif()
    target_compile_definitions(project_lib PUBLIC PROJECT_HAS_CHRONO=1)
    target_compile_definitions(project_lib PRIVATE
        PROJECT_CHRONO_CHQUATERNION_HEADER=\"chrono/core/ChQuaternion.h\"
        PROJECT_CHRONO_CHVECTOR_HEADER=\"chrono/core/ChVector3.h\"
        PROJECT_CHRONO_CHBODY_HEADER=\"chrono/physics/ChBody.h\"
        PROJECT_CHRONO_CHSYSTEMNSC_HEADER=\"chrono/physics/ChSystemNSC.h\"
    )
else()
    target_compile_definitions(project_lib PUBLIC PROJECT_HAS_CHRONO=0)
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
    target_compile_definitions(project_lib PRIVATE PROJECT_TEST_HOOKS=1)
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
        tests/unit/test_aero_runtime_models.cpp
        tests/unit/test_hydro_runtime_models.cpp
        tests/unit/test_run_analysis.cpp
        tests/unit/test_state_advancement.cpp
        tests/unit/test_state_advancement_startup_validation.cpp
        tests/unit/test_state_advancement_validation.cpp
        tests/unit/test_state_advancement_sundials.cpp
        tests/unit/test_simulator_config.cpp
        tests/unit/test_simulator_config_runtime.cpp
        tests/unit/test_simulator_config_mechanics.cpp
        tests/unit/test_simulation_run.cpp
        tests/unit/test_simulation_run_cli.cpp
        tests/unit/test_simulation_run_advancer.cpp
        tests/unit/test_simulation_run_state_validation.cpp
        tests/unit/test_simulation_run_state_validation_additional.cpp
        tests/unit/test_run_outputs.cpp
        tests/unit/test_run_outputs_hdf5.cpp
        tests/unit/test_run_outputs_config.cpp
        tests/unit/test_scenario_harness.cpp
        tests/unit/test_scenario_harness_loader.cpp
        tests/unit/test_scenario_harness_headwind.cpp
        tests/unit/test_scenario_harness_crosswind.cpp
        tests/unit/test_scenario_harness_additional.cpp
    )
    target_link_libraries(project_tests_unit PRIVATE project_lib GTest::gtest GTest::gtest_main Threads::Threads)
    set_project_warnings(project_tests_unit)
    enable_sanitizers(project_tests_unit)
    target_compile_definitions(project_tests_unit PRIVATE
        PROJECT_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
        PROJECT_TEST_HOOKS=1
    )

    add_executable(project_tests_integration
        tests/integration/test_hydro_runtime_integration.cpp
        tests/integration/test_aero_runtime_integration.cpp
        tests/integration/test_simulator_config_integration.cpp
        tests/integration/test_orchestrator_integration.cpp
        tests/integration/test_run_outputs_integration.cpp
    )
    target_link_libraries(project_tests_integration PRIVATE project_lib GTest::gtest GTest::gtest_main Threads::Threads)
    set_project_warnings(project_tests_integration)
    enable_sanitizers(project_tests_integration)
    target_compile_definitions(project_tests_integration PRIVATE PROJECT_SOURCE_DIR="${CMAKE_SOURCE_DIR}")

    add_executable(project_tests_system
        tests/system/test_calm_water_scenario_system.cpp
        tests/system/test_examples_system.cpp
        tests/system/test_simulator_config_system.cpp
        tests/system/test_run_analysis_system.cpp
        tests/system/test_headless_simulation_system.cpp
        tests/system/test_headless_outputs_system.cpp
        tests/system/test_scenario_harness_system.cpp
        tests/system/test_wind_scenario_system.cpp
        tests/system/test_v0_1_closure_system.cpp
    )
    target_link_libraries(project_tests_system PRIVATE project_lib GTest::gtest GTest::gtest_main Threads::Threads)
    set_project_warnings(project_tests_system)
    enable_sanitizers(project_tests_system)
    target_compile_definitions(
        project_tests_system PRIVATE
        PROJECT_APP_PATH="$<TARGET_FILE:project_app>"
        PROJECT_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
    )

    add_executable(project_tests_aux_headers
        tests/aux/include_aero_providers.cpp
        tests/aux/header_self_containment_main.cpp
        tests/aux/include_cli.cpp
        tests/aux/include_geometry.cpp
        tests/aux/include_hydro_provider.cpp
        tests/aux/include_hydro_providers.cpp
        tests/aux/include_mechanics_state.cpp
        tests/aux/include_run_analysis.cpp
        tests/aux/include_run_output.cpp
        tests/aux/include_run_result.cpp
        tests/aux/include_scenario_harness.cpp
        tests/aux/include_backend_catalog.cpp
        tests/aux/include_state_advancement.cpp
        tests/aux/include_simulation_run.cpp
        tests/aux/include_simulator_config.cpp
    )
    target_link_libraries(project_tests_aux_headers PRIVATE project_lib)
    set_project_warnings(project_tests_aux_headers)
    enable_sanitizers(project_tests_aux_headers)

    include(GoogleTest)
    gtest_discover_tests(
        project_tests_unit
        TEST_LIST unit_tests_list
        DISCOVERY_MODE PRE_TEST
        PROPERTIES
            LABELS "unit"
            TIMEOUT 30
            ENVIRONMENT "${PROJECT_SANITIZER_TEST_ENV}"
    )
    gtest_discover_tests(
        project_tests_integration
        TEST_LIST integration_tests_list
        DISCOVERY_MODE PRE_TEST
        PROPERTIES
            LABELS "integration"
            TIMEOUT 45
            ENVIRONMENT "${PROJECT_SANITIZER_TEST_ENV}"
    )
    gtest_discover_tests(
        project_tests_system
        TEST_LIST system_tests_list
        DISCOVERY_MODE PRE_TEST
        PROPERTIES
            LABELS "system"
            TIMEOUT 60
            ENVIRONMENT "${PROJECT_SANITIZER_TEST_ENV}"
    )
    add_test(NAME project_tests_aux_headers COMMAND project_tests_aux_headers)
    set_tests_properties(
        project_tests_aux_headers
        PROPERTIES
            LABELS "aux"
            TIMEOUT 15
            ENVIRONMENT "${PROJECT_SANITIZER_TEST_ENV}"
    )
endif()
