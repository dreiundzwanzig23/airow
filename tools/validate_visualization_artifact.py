#!/usr/bin/env python3
"""Validate airow visualization artifact JSON files."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import sys
from typing import Any

SCHEMA_ID = "airow.visualization.v1"


def _is_number(value: Any) -> bool:
    return isinstance(value, (int, float)) and not isinstance(value, bool)


def _require_object(value: Any, path: str, issues: list[str]) -> dict[str, Any]:
    if not isinstance(value, dict):
        issues.append(f"{path}: expected object")
        return {}
    return value


def _require_array(value: Any, path: str, issues: list[str]) -> list[Any]:
    if not isinstance(value, list):
        issues.append(f"{path}: expected array")
        return []
    return value


def _validate_vector_channel(channel: Any, path: str, issues: list[str]) -> None:
    channel_object = _require_object(channel, path, issues)
    values = _require_array(channel_object.get("value"), f"{path}.value", issues)
    if len(values) != 3 or not all(_is_number(value) for value in values):
        issues.append(f"{path}.value: expected three numeric components")
    for field in ("unit", "frame"):
        if not isinstance(channel_object.get(field), str) or not channel_object[field]:
            issues.append(f"{path}.{field}: expected non-empty string")


def validate_document(document: Any) -> list[str]:
    issues: list[str] = []
    root = _require_object(document, "$", issues)
    if root.get("schema_id") != SCHEMA_ID:
        issues.append(f"$.schema_id: expected {SCHEMA_ID}")

    metadata = _require_object(root.get("metadata"), "$.metadata", issues)
    for field in ("config_id", "providers", "trust_envelope"):
        if field not in metadata:
            issues.append(f"$.metadata.{field}: required")

    frames = _require_object(root.get("frames"), "$.frames", issues)
    world = _require_object(frames.get("world"), "$.frames.world", issues)
    if world.get("axes") != "x_forward_y_starboard_z_up":
        issues.append("$.frames.world.axes: expected x_forward_y_starboard_z_up")

    channels = _require_object(root.get("channels"), "$.channels", issues)
    for field in ("hull_pose", "hull_hydro_force"):
        if field not in channels:
            issues.append(f"$.channels.{field}: required")

    samples = _require_array(root.get("samples"), "$.samples", issues)
    if not samples:
        issues.append("$.samples: expected at least one sample")

    for sample_index, sample in enumerate(samples):
        sample_path = f"$.samples[{sample_index}]"
        sample_object = _require_object(sample, sample_path, issues)
        if not _is_number(sample_object.get("time_s")):
            issues.append(f"{sample_path}.time_s: expected number")
        _require_object(sample_object.get("transforms"), f"{sample_path}.transforms", issues)
        vectors = _require_object(sample_object.get("vectors"), f"{sample_path}.vectors", issues)
        for name, channel in vectors.items():
            _validate_vector_channel(channel, f"{sample_path}.vectors.{name}", issues)

    return issues


def validate_file(path: Path) -> list[str]:
    try:
        document = json.loads(path.read_text(encoding="utf-8"))
    except OSError as exc:
        return [f"{path}: failed to read file: {exc}"]
    except json.JSONDecodeError as exc:
        return [f"{path}: invalid JSON: {exc}"]
    return validate_document(document)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Validate an airow visualization artifact JSON file."
    )
    parser.add_argument("artifact", type=Path)
    args = parser.parse_args(argv)

    issues = validate_file(args.artifact)
    if issues:
        print("VISUALIZATION ARTIFACT INVALID", file=sys.stderr)
        for issue in issues:
            print(f"- {issue}", file=sys.stderr)
        return 1

    print("Visualization artifact OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
