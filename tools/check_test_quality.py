#!/usr/bin/env python3
"""Static policy checks for test quality and determinism."""

from __future__ import annotations

import pathlib
import re
import sys
from dataclasses import dataclass
from typing import Iterable

ROOT = pathlib.Path(__file__).resolve().parents[1]
TESTS_DIR = ROOT / "tests"
CXX_SUFFIXES = {".cpp", ".cc", ".cxx", ".hpp", ".h"}


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
        text = path.read_text(encoding="utf-8", errors="ignore")
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
