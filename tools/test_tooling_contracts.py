#!/usr/bin/env python3
"""Repository tooling contract checks for build and test policy."""

from __future__ import annotations

import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]


def require_contains(text: str, needle: str, label: str) -> None:
    if needle not in text:
        raise AssertionError(f"missing {label}: expected to find {needle!r}")


def main() -> int:
    compiler_warnings = (ROOT / "cmake" / "CompilerWarnings.cmake").read_text(
        encoding="utf-8"
    )
    for flag in (
        "-Wshadow",
        "-Wconversion",
        "-Wsign-conversion",
        "-Wfloat-conversion",
        "-Wnon-virtual-dtor",
    ):
        require_contains(compiler_warnings, flag, "compiler warning flag")

    sanitizers = (ROOT / "cmake" / "Sanitizers.cmake").read_text(encoding="utf-8")
    for token in ("ENABLE_TSAN", "ENABLE_STDLIB_ASSERTIONS", "-fno-omit-frame-pointer"):
        require_contains(sanitizers, token, "sanitizer policy")

    presets = json.loads((ROOT / "CMakePresets.json").read_text(encoding="utf-8"))
    configure_names = {preset["name"] for preset in presets["configurePresets"]}
    build_names = {preset["name"] for preset in presets["buildPresets"]}
    test_names = {preset["name"] for preset in presets["testPresets"]}
    test_presets = {preset["name"]: preset for preset in presets["testPresets"]}

    if "sanitized-clang-libcxx" not in configure_names:
        raise AssertionError("missing configure preset sanitized-clang-libcxx")
    if "sanitized-clang-libcxx" not in build_names:
        raise AssertionError("missing build preset sanitized-clang-libcxx")
    if "sanitized-clang-libcxx" not in test_names:
        raise AssertionError("missing test preset sanitized-clang-libcxx")
    if "gcc" not in configure_names:
        raise AssertionError("missing configure preset gcc")
    if "gcc" not in build_names:
        raise AssertionError("missing build preset gcc")
    if "gcc" not in test_names:
        raise AssertionError("missing test preset gcc")

    for preset_name in ("release", "release-clang-libcxx"):
        if preset_name not in test_names:
            raise AssertionError(f"missing release test preset {preset_name}")

    for preset_name in ("gcc", "sanitized-clang-libcxx"):
        discovery_env = test_presets[preset_name].get("environment", {})
        if discovery_env.get("ASAN_OPTIONS") != "detect_leaks=0:alloc_dealloc_mismatch=0":
            raise AssertionError(
                f"{preset_name} test preset must disable leak detection and alloc/dealloc mismatch during discovery"
            )
        if discovery_env.get("LSAN_OPTIONS") != "detect_leaks=0":
            raise AssertionError(
                f"{preset_name} test preset must disable leak detection during discovery"
            )

    test_script = (ROOT / "scripts" / "test.sh").read_text(encoding="utf-8")
    require_contains(test_script, "./scripts/test_sanitized.sh", "sanitized test lane hook")
    require_contains(test_script, "./scripts/test_gcc.sh", "GCC test lane hook")
    require_contains(
        test_script,
        'validation_run_logged "test-aux" ./scripts/test_aux.sh',
        "repo-wide aux lane hook in full test gate",
    )

    test_tdd_script = (ROOT / "scripts" / "test_tdd.sh").read_text(encoding="utf-8")
    require_contains(
        test_tdd_script,
        "./scripts/check_rgr_evidence.sh",
        "RGR evidence hook in test_tdd lane",
    )

    verify_script = (ROOT / "scripts" / "verify.sh").read_text(encoding="utf-8")
    require_contains(
        verify_script,
        "./scripts/check_rgr_evidence.sh",
        "RGR evidence hook in verify lane",
    )
    require_contains(verify_script, "./scripts/test_tdd.sh", "test_tdd gate in verify lane")
    if verify_script.find("./scripts/test_tdd.sh") > verify_script.find("./scripts/test.sh"):
        raise AssertionError("verify lane must run test_tdd before test.sh")

    rgr_script = (ROOT / "scripts" / "check_rgr_evidence.sh").read_text(encoding="utf-8")
    for marker in ("rgr:red", "rgr:green", "rgr:refactor"):
        require_contains(rgr_script, marker, "RGR marker contract")
    require_contains(rgr_script, 'mode="${RGR_ENFORCEMENT_MODE:-strict}"', "strict default RGR mode")

    test_aux_script = (ROOT / "scripts" / "test_aux.sh").read_text(encoding="utf-8")
    require_contains(test_aux_script, "./scripts/lint_tests.sh", "test lint hook")
    require_contains(
        test_aux_script,
        "python3 tools/test_validation_output.py",
        "validation output regression hook",
    )

    test_lint_script = (ROOT / "scripts" / "lint_tests.sh").read_text(encoding="utf-8")
    require_contains(test_lint_script, "TEST_LIZARD_CCN_THRESHOLD:-8", "test CCN threshold")
    require_contains(
        test_lint_script,
        "TEST_LIZARD_FUNCTION_LENGTH_THRESHOLD:-75",
        "test function length threshold",
    )
    require_contains(
        test_lint_script,
        "TEST_LIZARD_PARAMETER_THRESHOLD:-4",
        "test parameter threshold",
    )

    test_quality_tool = (ROOT / "tools" / "check_test_quality.py").read_text(
        encoding="utf-8"
    )
    for token in (
        "FRIEND_TEST",
        "sleep_for",
        "system_clock",
        "implementation .cpp files",
        "private or protected access hacks",
    ):
        require_contains(test_quality_tool, token, "test quality pattern")
    require_contains(
        test_quality_tool,
        "TEST_FILE_LINE_LIMIT_OVERRIDES: dict[str, int] = {}",
        "empty test file line override set",
    )
    require_contains(
        test_quality_tool,
        "TEST_CASE_LIMIT_OVERRIDES: dict[str, int] = {}",
        "empty test case override set",
    )

    clang_tidy = (ROOT / ".clang-tidy").read_text(encoding="utf-8")
    require_contains(clang_tidy, "misc-include-cleaner", "clang-tidy include-cleaner check")

    depcheck = (ROOT / "tools" / "depcheck.py").read_text(encoding="utf-8")
    require_contains(depcheck, "Internal component include violation", "public-header-only architecture check")
    require_contains(depcheck, "Component cycle detected", "component cycle detection")
    require_contains(depcheck, "Orphan component evidence", "component orphan evidence check")

    dependency_rules = (ROOT / "docs" / "process" / "DEPENDENCY_RULES.md").read_text(
        encoding="utf-8"
    )
    require_contains(
        dependency_rules,
        "Cross-component access must go through `include/project/**` public headers.",
        "dependency-rules public header policy",
    )
    require_contains(
        dependency_rules,
        "actual realized component include graph must remain acyclic",
        "dependency-rules cycle policy",
    )

    cmake_lists = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
    for timeout in ("TIMEOUT 30", "TIMEOUT 45", "TIMEOUT 60"):
        require_contains(cmake_lists, timeout, "CTest timeout")
    require_contains(
        cmake_lists,
        "gtest_discover_tests(",
        "GoogleTest runtime discovery policy",
    )
    require_contains(
        cmake_lists,
        "DISCOVERY_MODE PRE_TEST",
        "GoogleTest discovery mode policy",
    )
    if "gtest_add_tests(" in cmake_lists:
        raise AssertionError("GoogleTest source-scanned discovery is not allowed")
    require_contains(cmake_lists, "project_tests_aux_headers", "aux header self-containment target")
    require_contains(cmake_lists, 'LABELS "aux"', "aux CTest label")
    require_contains(
        cmake_lists,
        "alloc_dealloc_mismatch=0",
        "libc++ sanitizer exception policy",
    )
    require_contains(
        cmake_lists,
        ".external/chrono-install",
        "repo-managed Chrono prefix policy",
    )
    require_contains(
        cmake_lists,
        "Chrono support is required for the standard build.",
        "standard-build Chrono requirement policy",
    )

    setup_script = (ROOT / "scripts" / "setup.sh").read_text(encoding="utf-8")
    require_contains(
        setup_script,
        'STDLIB="libstdc++"',
        "libstdc++ standard setup default",
    )
    require_contains(
        setup_script,
        '"${SCRIPT_DIR}/setup_chrono.sh"',
        "Chrono provisioning hook in setup",
    )

    setup_chrono_script = (ROOT / "scripts" / "setup_chrono.sh").read_text(
        encoding="utf-8"
    )
    require_contains(
        setup_chrono_script,
        ".external/chrono-install",
        "supported Chrono install prefix",
    )
    require_contains(
        setup_chrono_script,
        "BUILD_DEMOS=OFF",
        "minimal Chrono build policy",
    )
    require_contains(
        setup_chrono_script,
        "CH_ENABLE_MODULE_IRRLICHT=OFF",
        "Chrono module minimization policy",
    )

    state_advancement = (ROOT / "src" / "lib" / "numerics" / "state_advancement.cpp").read_text(
        encoding="utf-8"
    )
    if "AIROW_SUNDIALS_TEST_FAULT" in state_advancement:
        raise AssertionError("production state advancement must not depend on AIROW_SUNDIALS_TEST_FAULT")

    print("Tooling contracts OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
