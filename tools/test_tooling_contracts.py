#!/usr/bin/env python3
"""Repository tooling contract checks for build and test policy."""

from __future__ import annotations

import contextlib
import importlib.util
import io
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def require_contains(text: str, needle: str, label: str) -> None:
    if needle not in text:
        raise AssertionError(f"missing {label}: expected to find {needle!r}")


def load_module(path: Path, name: str):
    spec = importlib.util.spec_from_file_location(name, path)
    if spec is None or spec.loader is None:
        raise AssertionError(f"could not load module from {path}")
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    spec.loader.exec_module(module)
    return module


def check_compact_traceability_report() -> None:
    tracecheck_path = ROOT / "tools" / "tracecheck.py"
    tracecheck = load_module(tracecheck_path, "airow_tracecheck_contract")
    traceability_path = ROOT / "docs" / "process" / "TRACEABILITY.md"
    original = traceability_path.read_text(encoding="utf-8")
    try:
        data = tracecheck.collect_data()
        warnings = tracecheck.numbering_warnings(data) + tracecheck.architecture_warnings(data)
        tracecheck.write_traceability(data, warnings)
        compact_report = traceability_path.read_text(encoding="utf-8")
    finally:
        traceability_path.write_text(original, encoding="utf-8")

    for token in (
        "## Summary",
        "## Evidence Coverage",
        "## Full Trace Data",
        "`python3 tools/tracecheck.py --json`",
    ):
        require_contains(compact_report, token, "compact traceability report")
    if "| Requirement | Req Status | Satisfied by A |" in compact_report:
        raise AssertionError("TRACEABILITY.md must not contain the full trace matrix")
    if "## Architecture Details" in compact_report:
        raise AssertionError("TRACEABILITY.md must not duplicate ARCHITECTURE.md details")


def check_visualization_artifact_validator() -> None:
    validator_path = ROOT / "tools" / "validate_visualization_artifact.py"
    validator = load_module(validator_path, "airow_visualization_validator_contract")
    valid_artifact = {
        "schema_id": "airow.visualization.v1",
        "metadata": {
            "config_id": "tool-contract",
            "providers": {},
            "trust_envelope": {},
        },
        "frames": {"world": {"axes": "x_forward_y_starboard_z_up"}},
        "channels": {"hull_pose": {}, "hull_hydro_force": {}},
        "samples": [
            {
                "time_s": 0.0,
                "transforms": {},
                "vectors": {
                    "hull_hydro_force_world_n": {
                        "value": [0.0, 0.0, 0.0],
                        "unit": "N",
                        "frame": "world",
                    }
                },
            }
        ],
    }
    if validator.validate_document(valid_artifact):
        raise AssertionError("valid visualization artifact was rejected")

    wrong_schema = dict(valid_artifact)
    wrong_schema["schema_id"] = "airow.visualization.v0"
    if not validator.validate_document(wrong_schema):
        raise AssertionError("unsupported visualization artifact schema was accepted")

    missing_vector_unit = json.loads(json.dumps(valid_artifact))
    del missing_vector_unit["samples"][0]["vectors"]["hull_hydro_force_world_n"][
        "unit"
    ]
    if not validator.validate_document(missing_vector_unit):
        raise AssertionError("malformed visualization vector channel was accepted")


def run_test_quality_with_fixture(
    relative_dir: str, content: str, *, changed_scope: bool
) -> tuple[int, str]:
    check_test_quality = load_module(
        ROOT / "tools" / "check_test_quality.py",
        "airow_check_test_quality_contract",
    )
    fixture_root = ROOT / relative_dir
    with tempfile.TemporaryDirectory(dir=fixture_root) as temp_dir:
        path = Path(temp_dir) / "test_contract_fixture.cpp"
        path.write_text(content, encoding="utf-8")
        old_scope = os.environ.get("TEST_LINT_SCOPE")
        if changed_scope:
            os.environ["TEST_LINT_SCOPE"] = "changed"
        elif old_scope is not None:
            del os.environ["TEST_LINT_SCOPE"]
        try:
            output = io.StringIO()
            with contextlib.redirect_stdout(output):
                rc = check_test_quality.main([str(path)])
        finally:
            if old_scope is None:
                os.environ.pop("TEST_LINT_SCOPE", None)
            else:
                os.environ["TEST_LINT_SCOPE"] = old_scope
    return rc, relative_dir


def unit_test_fixture(*, extra_tags: str = "") -> str:
    return f"""
#include <gtest/gtest.h>

/**
 * @test UT-999
 * @verifies [D-999]
{extra_tags} * @notes Given one input, when the behavior is evaluated, then the
 * single observable output is exact.
 */
TEST(ToolingContractFixture, ChecksOneObservableBehavior) {{
  EXPECT_EQ(2, 1 + 1);
}}
"""


def check_changed_unit_test_authoring_contract() -> None:
    missing_tags_rc, _ = run_test_quality_with_fixture(
        "tests/unit",
        unit_test_fixture(),
        changed_scope=True,
    )
    if missing_tags_rc == 0:
        raise AssertionError(
            "changed unit tests without @case and @oracle tags must be rejected"
        )

    valid_rc, _ = run_test_quality_with_fixture(
        "tests/unit",
        unit_test_fixture(extra_tags=" * @case nominal\n * @oracle exact\n"),
        changed_scope=True,
    )
    if valid_rc != 0:
        raise AssertionError("changed unit tests with one @case and one @oracle must pass")

    legacy_scope_rc, _ = run_test_quality_with_fixture(
        "tests/unit",
        unit_test_fixture(),
        changed_scope=False,
    )
    if legacy_scope_rc != 0:
        raise AssertionError("full-suite test quality must keep accepting legacy UT blocks")

    integration_rc, _ = run_test_quality_with_fixture(
        "tests/integration",
        unit_test_fixture().replace("@test UT-999", "@test IT-999").replace(
            "@verifies [D-999]", "@verifies [A-999]"
        ),
        changed_scope=True,
    )
    if integration_rc != 0:
        raise AssertionError("IT fixtures must not require UT-only @case/@oracle tags")

    system_rc, _ = run_test_quality_with_fixture(
        "tests/system",
        unit_test_fixture().replace("@test UT-999", "@test QT-999").replace(
            "@verifies [D-999]", "@verifies [R-999]"
        ),
        changed_scope=True,
    )
    if system_rc != 0:
        raise AssertionError("QT fixtures must not require UT-only @case/@oracle tags")


def run_rgr_check(evidence: str) -> subprocess.CompletedProcess[str]:
    env = os.environ.copy()
    env["RGR_ENFORCEMENT_MODE"] = "strict"
    env["RGR_EVIDENCE_TEXT"] = evidence
    return subprocess.run(
        [str(ROOT / "scripts" / "check_rgr_evidence.sh")],
        cwd=ROOT,
        env=env,
        text=True,
        capture_output=True,
        check=False,
    )


def check_rgr_evidence_order_contract() -> None:
    valid_multi_slice = """
rgr:red:
- failing test for first behavior
rgr:green:
- minimal implementation
rgr:refactor:
- cleanup
rgr:red:
- failing test for second behavior
rgr:green:
- minimal implementation
rgr:refactor:
- no-op rationale
"""
    if run_rgr_check(valid_multi_slice).returncode != 0:
        raise AssertionError("ordered multi-slice RGR evidence must pass")

    out_of_order = """
rgr:red:
- failing test
rgr:refactor:
- cleanup before green
rgr:green:
- implementation
"""
    out_of_order_result = run_rgr_check(out_of_order)
    if out_of_order_result.returncode == 0:
        raise AssertionError("out-of-order RGR evidence must fail")
    require_contains(
        out_of_order_result.stdout + out_of_order_result.stderr,
        "out-of-order",
        "RGR order diagnostic",
    )

    incomplete_slice = """
rgr:red:
- failing test
rgr:green:
- implementation
"""
    incomplete_result = run_rgr_check(incomplete_slice)
    if incomplete_result.returncode == 0:
        raise AssertionError("incomplete RGR evidence slices must fail")
    require_contains(
        incomplete_result.stdout + incomplete_result.stderr,
        "incomplete-slice",
        "RGR incomplete-slice diagnostic",
    )


def main() -> int:
    check_compact_traceability_report()
    check_visualization_artifact_validator()
    check_changed_unit_test_authoring_contract()
    check_rgr_evidence_order_contract()

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
    require_contains(
        test_script,
        'validation_run_logged "test-performance" ./scripts/test_performance.sh',
        "repo-wide performance lane hook in full test gate",
    )

    test_tdd_script = (ROOT / "scripts" / "test_tdd.sh").read_text(encoding="utf-8")
    require_contains(
        test_tdd_script,
        "./scripts/check_rgr_evidence.sh",
        "RGR evidence hook in test_tdd lane",
    )
    if "./scripts/test_performance.sh" in test_tdd_script:
        raise AssertionError("test_tdd lane must not run the performance lane")

    verify_script = (ROOT / "scripts" / "verify.sh").read_text(encoding="utf-8")
    require_contains(
        verify_script,
        "./scripts/check_rgr_evidence.sh",
        "RGR evidence hook in verify lane",
    )
    require_contains(
        verify_script,
        "./scripts/test_performance.sh",
        "performance gate in verify lane",
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
