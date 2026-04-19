#!/usr/bin/env python3
"""Protected-scenario budget manifest helper and evaluator."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import sys


def load_manifest(path: Path) -> dict:
    manifest = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(manifest, dict):
        raise ValueError("manifest root must be an object")
    if manifest.get("schema_id") != "scenario_performance_budgets.v1":
        raise ValueError("manifest schema_id must be scenario_performance_budgets.v1")
    budgets = manifest.get("scenario_budgets")
    if not isinstance(budgets, list) or not budgets:
        raise ValueError("manifest scenario_budgets must be a non-empty array")
    for index, entry in enumerate(budgets):
        if not isinstance(entry, dict):
            raise ValueError(f"scenario_budgets[{index}] must be an object")
        for field in ("scenario_id", "step_name", "ctest_regex"):
            if not isinstance(entry.get(field), str) or not entry[field]:
                raise ValueError(f"scenario_budgets[{index}].{field} must be a non-empty string")
        if not isinstance(entry.get("max_duration_seconds"), int) or entry["max_duration_seconds"] <= 0:
            raise ValueError(
                f"scenario_budgets[{index}].max_duration_seconds must be a positive integer"
            )
    return manifest


def parse_steps_tsv(path: Path) -> dict[str, dict]:
    samples: dict[str, dict] = {}
    if not path.exists():
        return samples

    for raw_line in path.read_text(encoding="utf-8").splitlines():
        if not raw_line:
            continue
        columns = raw_line.split("\t")
        if len(columns) != 5:
            raise ValueError(f"invalid steps TSV row: {raw_line!r}")
        step_name, status, exit_code, log_path, duration_seconds = columns
        samples[step_name] = {
            "step_name": step_name,
            "status": status,
            "exit_code": int(exit_code),
            "log_path": log_path,
            "duration_seconds": int(duration_seconds),
        }
    return samples


def evaluate_manifest(
    manifest: dict, manifest_path: Path, samples: dict[str, dict], steps_path: Path
) -> tuple[dict, int]:
    scenario_results = []
    issues = []

    for index, budget in enumerate(manifest["scenario_budgets"]):
        sample = samples.get(budget["step_name"])
        scenario_result = {
            "scenario_id": budget["scenario_id"],
            "step_name": budget["step_name"],
            "ctest_regex": budget["ctest_regex"],
            "max_duration_seconds": budget["max_duration_seconds"],
            "status": "pass",
            "duration_seconds": None,
            "log_path": "",
        }

        if sample is None:
            scenario_result["status"] = "fail"
            issues.append(
                {
                    "code": "performance_budget_missing_step",
                    "path": f"$.scenario_budgets[{index}].step_name",
                    "message": f"missing validation step for protected scenario '{budget['scenario_id']}'",
                }
            )
            scenario_results.append(scenario_result)
            continue

        scenario_result["duration_seconds"] = sample["duration_seconds"]
        scenario_result["log_path"] = sample["log_path"]

        if sample["status"] != "pass" or sample["exit_code"] != 0:
            scenario_result["status"] = "fail"
            issues.append(
                {
                    "code": "performance_budget_step_failed",
                    "path": f"$.scenario_budgets[{index}].step_name",
                    "message": f"validation step for protected scenario '{budget['scenario_id']}' did not pass before budget evaluation",
                }
            )
            scenario_results.append(scenario_result)
            continue

        if sample["duration_seconds"] > budget["max_duration_seconds"]:
            scenario_result["status"] = "fail"
            issues.append(
                {
                    "code": "performance_budget_exceeded",
                    "path": f"$.scenario_budgets[{index}].max_duration_seconds",
                    "message": f"protected scenario '{budget['scenario_id']}' exceeded its documented runtime budget",
                }
            )

        scenario_results.append(scenario_result)

    payload = {
        "status": "pass" if not issues else "fail",
        "manifest": {
            "path": str(manifest_path.resolve()),
            "schema_id": manifest["schema_id"],
            "development_environment_class": manifest["development_environment_class"],
        },
        "summary_steps_path": str(steps_path),
        "scenario_results": scenario_results,
        "issues": issues,
    }
    return payload, 0 if not issues else 1


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--list-steps", action="store_true")
    parser.add_argument("--steps-tsv", type=Path)
    parser.add_argument("--output", type=Path)
    return parser


def main() -> int:
    parser = build_parser()
    args = parser.parse_args()

    try:
        manifest = load_manifest(args.manifest)
    except (OSError, ValueError, json.JSONDecodeError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 2

    if args.list_steps:
        for budget in manifest["scenario_budgets"]:
            print(f"{budget['step_name']}\t{budget['ctest_regex']}")
        return 0

    if args.steps_tsv is None or args.output is None:
        parser.error("--steps-tsv and --output are required unless --list-steps is used")

    try:
        samples = parse_steps_tsv(args.steps_tsv)
        report, rc = evaluate_manifest(manifest, args.manifest, samples, args.steps_tsv)
        args.output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
        return rc
    except (OSError, ValueError, json.JSONDecodeError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
