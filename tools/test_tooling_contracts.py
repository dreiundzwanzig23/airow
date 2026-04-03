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

    test_script = (ROOT / "scripts" / "test.sh").read_text(encoding="utf-8")
    require_contains(test_script, "./scripts/test_sanitized.sh", "sanitized test lane hook")
    require_contains(test_script, "./scripts/test_gcc.sh", "GCC test lane hook")

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
    require_contains(cmake_lists, "project_tests_aux_headers", "aux header self-containment target")
    require_contains(cmake_lists, 'LABELS "aux"', "aux CTest label")

    print("Tooling contracts OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
