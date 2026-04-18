#!/usr/bin/env python3
"""Static policy checks for test quality and determinism."""

from __future__ import annotations

import os
import pathlib
import re
import sys
from dataclasses import dataclass
from typing import Iterable

ROOT = pathlib.Path(__file__).resolve().parents[1]
TESTS_DIR = ROOT / "tests"
CXX_SUFFIXES = {".cpp", ".cc", ".cxx", ".hpp", ".h"}
DEFAULT_MAX_TEST_FILE_LINES = int(os.environ.get("TEST_MAX_FILE_LINES", "900"))
DEFAULT_MAX_TEST_CASES_PER_FILE = int(os.environ.get("TEST_MAX_TEST_CASES", "14"))
TEST_FILE_LINE_LIMIT_OVERRIDES: dict[str, int] = {}
TEST_CASE_LIMIT_OVERRIDES: dict[str, int] = {}
TEST_CASE_PATTERN = re.compile(r"^\s*TEST(?:_F|_P)?\s*\(", re.M)


@dataclass(frozen=True)
class PolicyPattern:
    description: str
    pattern: re.Pattern[str]


POLICY_PATTERNS = (
    PolicyPattern(
        "tests must not include implementation .cpp files",
        re.compile(r'^\s*#include\s+"[^"]+\.cpp"', re.M),
    ),
    PolicyPattern(
        "tests must not use private or protected access hacks",
        re.compile(r"^\s*#\s*define\s+(?:private|protected)\s+public\b", re.M),
    ),
    PolicyPattern(
        "tests must not rely on FRIEND_TEST backdoors",
        re.compile(r"\bFRIEND_TEST\s*\("),
    ),
    PolicyPattern(
        "tests must not use sleep-based timing",
        re.compile(
            r"\b(?:std::this_thread::sleep_for|std::this_thread::sleep_until|sleep|usleep|nanosleep)\s*\("
        ),
    ),
    PolicyPattern(
        "tests must not read the wall clock directly",
        re.compile(
            r"\bstd::chrono::(?:system_clock|steady_clock|high_resolution_clock)::now\s*\("
        ),
    ),
    PolicyPattern(
        "tests must not use direct time() calls",
        re.compile(r"\b(?:std::)?time\s*\("),
    ),
    PolicyPattern(
        "tests must not depend on generic TEST preprocessor hooks",
        re.compile(
            r"^\s*#\s*ifn?def\s+TEST\b|^\s*#\s*if\s+defined\s*\(\s*TEST\s*\)",
            re.M,
        ),
    ),
)


def candidate_files(argv: list[str]) -> Iterable[pathlib.Path]:
    if argv:
        for arg in argv:
            path = pathlib.Path(arg)
            if not path.is_absolute():
                path = ROOT / path
            if path.is_file() and path.suffix in CXX_SUFFIXES:
                yield path
        return

    for path in TESTS_DIR.rglob("*"):
        if path.is_file() and path.suffix in CXX_SUFFIXES:
            yield path


def line_number(text: str, offset: int) -> int:
    return text.count("\n", 0, offset) + 1


def main(argv: list[str]) -> int:
    problems: list[str] = []

    for path in candidate_files(argv):
        rel_path = path.relative_to(ROOT)
        rel_key = rel_path.as_posix()
        text = path.read_text(encoding="utf-8", errors="ignore")

        line_limit = TEST_FILE_LINE_LIMIT_OVERRIDES.get(
            rel_key, DEFAULT_MAX_TEST_FILE_LINES
        )
        line_count = text.count("\n") + 1
        if line_count > line_limit:
            problems.append(
                f"{rel_path}: file has {line_count} lines (limit {line_limit})"
            )

        test_case_limit = TEST_CASE_LIMIT_OVERRIDES.get(
            rel_key, DEFAULT_MAX_TEST_CASES_PER_FILE
        )
        test_case_count = len(TEST_CASE_PATTERN.findall(text))
        if test_case_count > test_case_limit:
            problems.append(
                f"{rel_path}: file has {test_case_count} TEST cases (limit {test_case_limit})"
            )

        for policy in POLICY_PATTERNS:
            for match in policy.pattern.finditer(text):
                problems.append(
                    f"{rel_path}:{line_number(text, match.start())}: {policy.description}"
                )

    if problems:
        print("TEST QUALITY CHECK FAILED:")
        for problem in problems:
            print(f" - {problem}")
        return 1

    print("Test quality checks OK")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
