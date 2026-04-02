#!/usr/bin/env python3
"""Dependency guard checker based on include relations."""

from __future__ import annotations

import pathlib
import re
import sys
from typing import Dict, Iterable, List, Optional, Set

ROOT = pathlib.Path(__file__).resolve().parents[1]
RULES_FILE = ROOT / "docs" / "process" / "DEPENDENCY_RULES.md"
INCLUDE_RE = re.compile(r'^\s*#include\s+"(?P<path>[^"]+)"')
RULE_RE = re.compile(r"^-\s*`(?P<src>[^`]+?)\s*->\s*(?P<dst>[^`]+)`\s*$")
COMPONENT_PREFIXES = {
    "core": ("include/project/core/", "src/lib/core/"),
    "configuration": ("include/project/configuration/", "src/lib/configuration/"),
    "orchestrator": ("include/project/orchestrator/", "src/lib/orchestrator/"),
    "mechanics": ("include/project/mechanics/", "src/lib/mechanics/"),
    "hydro": ("include/project/hydro/", "src/lib/hydro/"),
    "aero": ("include/project/aero/", "src/lib/aero/"),
    "control": ("include/project/control/", "src/lib/control/"),
    "output": ("include/project/output/", "src/lib/output/"),
    "calibration": ("include/project/calibration/", "src/lib/calibration/"),
}


def classify(path: pathlib.Path) -> Optional[str]:
    rel = path.relative_to(ROOT).as_posix()
    if rel.startswith("include/"):
        return "include"
    if rel.startswith("src/lib/"):
        return "src/lib"
    if rel.startswith("src/app/"):
        return "src/app"
    if rel.startswith("tests/unit/"):
        return "tests/unit"
    if rel.startswith("tests/integration/"):
        return "tests/integration"
    if rel.startswith("tests/system/"):
        return "tests/system"
    return None


def classify_component(path: pathlib.Path) -> Optional[str]:
    rel = path.relative_to(ROOT).as_posix()
    for component, prefixes in COMPONENT_PREFIXES.items():
        if any(rel.startswith(prefix) for prefix in prefixes):
            return component
    return None


def parse_rules(section_heading: str) -> Dict[str, Set[str]]:
    allowed: Dict[str, Set[str]] = {}
    text = RULES_FILE.read_text(encoding="utf-8")
    in_allowed_section = False
    for line in text.splitlines():
        stripped = line.strip()
        if stripped.startswith("## "):
            in_allowed_section = stripped == section_heading
            continue
        if not in_allowed_section:
            continue
        match = RULE_RE.match(stripped)
        if not match:
            continue
        src = match.group("src").strip()
        destinations = {d.strip() for d in match.group("dst").split(",") if d.strip()}
        allowed[src] = destinations
    return allowed


def source_files() -> Iterable[pathlib.Path]:
    for root in (ROOT / "include", ROOT / "src", ROOT / "tests"):
        if not root.exists():
            continue
        for path in root.rglob("*"):
            if path.suffix in {".hpp", ".h", ".cpp", ".cc", ".cxx"}:
                yield path


def resolve_include(origin: pathlib.Path, include_path: str) -> Optional[pathlib.Path]:
    candidates: List[pathlib.Path] = []
    if include_path.startswith("project/"):
        candidates.append(ROOT / "include" / include_path)
    candidates.append(origin.parent / include_path)
    candidates.append(ROOT / "include" / include_path)
    candidates.append(ROOT / "src" / include_path)
    candidates.append(ROOT / "tests" / include_path)

    seen: Set[pathlib.Path] = set()
    for candidate in candidates:
        candidate = candidate.resolve()
        if candidate in seen:
            continue
        seen.add(candidate)
        if candidate.exists() and candidate.is_file():
            return candidate
    return None


def main() -> int:
    allowed = parse_rules("## Allowed dependencies")
    component_allowed = parse_rules("## Allowed component dependencies")
    problems: List[str] = []

    for src in source_files():
        src_group = classify(src)
        src_component = classify_component(src)
        if src_group is None:
            continue
        if src_group not in allowed:
            problems.append(f"No dependency rule for source group {src_group}")
            continue
        if src_component is not None and src_component not in component_allowed:
            problems.append(f"No component rule for source component {src_component}")
            continue

        text = src.read_text(encoding="utf-8", errors="ignore")
        for line in text.splitlines():
            include_match = INCLUDE_RE.match(line)
            if not include_match:
                continue
            included = include_match.group("path")
            resolved = resolve_include(src, included)
            if resolved is None:
                problems.append(f"{src.relative_to(ROOT)} includes unresolved path \"{included}\"")
                continue

            dst_group = classify(resolved)
            dst_component = classify_component(resolved)
            if dst_group is None:
                continue
            if dst_group not in allowed[src_group]:
                problems.append(
                    f"Dependency violation: {src.relative_to(ROOT)} ({src_group}) -> "
                    f"{resolved.relative_to(ROOT)} ({dst_group})"
                )
            if (
                src_component is not None
                and dst_component is not None
                and dst_component not in component_allowed[src_component]
            ):
                problems.append(
                    f"Component dependency violation: {src.relative_to(ROOT)} ({src_component}) -> "
                    f"{resolved.relative_to(ROOT)} ({dst_component})"
                )

    if problems:
        print("DEPCHECK FAILED:")
        for problem in problems:
            print(f" - {problem}")
        return 1

    print("Dependency rules OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
