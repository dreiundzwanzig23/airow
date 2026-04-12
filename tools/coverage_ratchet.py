#!/usr/bin/env python3
"""Fail when changed src/lib files regress coverage against a baseline export."""

from __future__ import annotations

import argparse
import json
import pathlib
import sys
from dataclasses import dataclass


@dataclass(frozen=True)
class CoverageMetrics:
    region_percent: float
    branch_percent: float


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--current-export", required=True)
    parser.add_argument("--baseline-export", required=True)
    parser.add_argument("--changed-file", action="append", default=[])
    parser.add_argument("--tolerance", type=float, default=0.01)
    return parser.parse_args()


def to_src_lib_relative(path: str) -> str | None:
    normalized = pathlib.Path(path).as_posix()
    marker = "/src/lib/"
    if marker in normalized:
        return "src/lib/" + normalized.split(marker, 1)[1]
    if normalized.startswith("src/lib/"):
        return normalized
    return None


def load_export(path: pathlib.Path) -> dict[str, CoverageMetrics]:
    with path.open(encoding="utf-8") as handle:
        payload = json.load(handle)

    files = payload.get("data", [{}])[0].get("files", [])
    metrics_by_file: dict[str, CoverageMetrics] = {}
    for entry in files:
        file_path = to_src_lib_relative(entry.get("filename", ""))
        if file_path is None:
            continue
        summary = entry.get("summary", {})
        regions = summary.get("regions", {})
        branches = summary.get("branches", {})
        metrics_by_file[file_path] = CoverageMetrics(
            region_percent=float(regions.get("percent", 0.0)),
            branch_percent=float(branches.get("percent", 0.0)),
        )
    return metrics_by_file


def canonicalize_changed_file(path: str) -> str | None:
    return to_src_lib_relative(path)


def main() -> int:
    args = parse_args()
    current = load_export(pathlib.Path(args.current_export))
    baseline = load_export(pathlib.Path(args.baseline_export))

    changed_files = [
        canonicalize_changed_file(path)
        for path in args.changed_file
        if path.startswith("src/lib/")
    ]
    changed_files = [path for path in changed_files if path is not None]

    if not changed_files:
        print("Coverage ratchet skipped: no changed src/lib files.")
        return 0

    problems: list[str] = []
    for changed_file in sorted(set(changed_files)):
        current_metrics = current.get(changed_file)
        if current_metrics is None:
            problems.append(
                f"{changed_file}: missing from current coverage export; ensure file is part of src/lib coverage scope."
            )
            continue

        baseline_metrics = baseline.get(changed_file)
        if baseline_metrics is None:
            continue

        if current_metrics.region_percent + args.tolerance < baseline_metrics.region_percent:
            problems.append(
                f"{changed_file}: region coverage regressed {baseline_metrics.region_percent:.2f}% -> {current_metrics.region_percent:.2f}%"
            )
        if current_metrics.branch_percent + args.tolerance < baseline_metrics.branch_percent:
            problems.append(
                f"{changed_file}: branch coverage regressed {baseline_metrics.branch_percent:.2f}% -> {current_metrics.branch_percent:.2f}%"
            )

    if problems:
        print("Coverage ratchet failed:")
        for problem in problems:
            print(f" - {problem}")
        return 1

    print("Coverage ratchet passed: no changed-file coverage regressions.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
