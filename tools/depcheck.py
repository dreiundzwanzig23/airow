#!/usr/bin/env python3
"""Dependency guard checker based on include relations and component ownership."""

from __future__ import annotations

import pathlib
import re
import sys
from typing import Dict, Iterable, List, Optional, Set

ROOT = pathlib.Path(__file__).resolve().parents[1]
RULES_FILE = ROOT / "docs" / "process" / "DEPENDENCY_RULES.md"
ARCHITECTURE_FILE = ROOT / "docs" / "process" / "ARCHITECTURE.md"
INCLUDE_RE = re.compile(r'^\s*#include\s+"(?P<path>[^"]+)"')
RULE_RE = re.compile(r"^-\s*`(?P<src>[^`]+?)\s*->\s*(?P<dst>[^`]+)`\s*$")
ARCH_ID_RE = re.compile(r"^#{2,6}\s+(?P<id>A-\d{3})\b", re.M)
FIELD_START = re.compile(r"^- \*\*(?P<key>[^*]+)\*\*:\s*(?P<val>.*)$")
BLOCK_RE = re.compile(r"/\*\*([\s\S]*?)\*/", re.M)
TAG_RE = re.compile(r"@(?P<k>test|verifies|aux)\s+(?P<v>.+)")
CXX_SUFFIXES = {".hpp", ".h", ".ipp", ".cpp", ".cc", ".cxx"}
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
COMPONENT_ARCHITECTURE = {
    "configuration": "A-001",
    "orchestrator": "A-002",
    "mechanics": "A-003",
    "hydro": "A-004",
    "aero": "A-005",
    "control": "A-006",
    "output": "A-007",
    "calibration": "A-009",
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


def parse_list_field(value: str) -> List[str]:
    value = value.strip()
    if not (value.startswith("[") and value.endswith("]")):
        return []
    inner = value[1:-1].strip()
    if not inner:
        return []
    return [part.strip() for part in inner.split(",") if part.strip()]


def parse_markdown_fields(block: str) -> Dict[str, str]:
    fields: Dict[str, str] = {}
    current_key: str | None = None

    for raw_line in block.splitlines():
        line = raw_line.rstrip()
        if not line:
            if current_key is not None:
                fields[current_key] = fields[current_key] + "\n"
            continue

        field_match = FIELD_START.match(line)
        if field_match:
            current_key = field_match.group("key").strip()
            fields[current_key] = field_match.group("val").strip()
            continue

        if current_key is not None and (line.startswith("  ") or line.startswith("\t")):
            continuation = line.strip()
            if continuation:
                existing = fields[current_key]
                fields[current_key] = (
                    f"{existing}\n{continuation}" if existing else continuation
                )
            continue

        current_key = None

    return {key: value.rstrip() for key, value in fields.items()}


def parse_architecture_items() -> Dict[str, Dict[str, str]]:
    text = ARCHITECTURE_FILE.read_text(encoding="utf-8")
    items: Dict[str, Dict[str, str]] = {}
    matches = list(re.finditer(ARCH_ID_RE, text))
    for index, match in enumerate(matches):
        item_id = match.group("id")
        start = match.end()
        end = matches[index + 1].start() if index + 1 < len(matches) else len(text)
        items[item_id] = parse_markdown_fields(text[start:end])
    return items


def parse_verified_architecture_ids() -> Set[str]:
    verified: Set[str] = set()
    test_root = ROOT / "tests"
    if not test_root.exists():
        return verified

    for path in test_root.rglob("*"):
        if path.suffix not in CXX_SUFFIXES or not path.is_file():
            continue
        text = path.read_text(encoding="utf-8", errors="ignore")
        for block_match in re.finditer(BLOCK_RE, text):
            tags: Dict[str, List[str]] = {}
            for tag_match in re.finditer(TAG_RE, block_match.group(1)):
                tags.setdefault(tag_match.group("k"), []).append(
                    tag_match.group("v").strip()
                )
            if "test" not in tags:
                continue
            aux_values = [value.lower() for value in tags.get("aux", [])]
            if any(value in {"yes", "true", "1"} for value in aux_values):
                continue
            for value in tags.get("verifies", []):
                for item in parse_list_field(value):
                    if item.startswith("A-"):
                        verified.add(item)
    return verified


def source_files() -> Iterable[pathlib.Path]:
    for root in (ROOT / "include", ROOT / "src", ROOT / "tests"):
        if not root.exists():
            continue
        for path in root.rglob("*"):
            if path.suffix in CXX_SUFFIXES:
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


def component_inventory() -> Dict[str, Dict[str, List[pathlib.Path]]]:
    inventory: Dict[str, Dict[str, List[pathlib.Path]]] = {}
    for component, prefixes in COMPONENT_PREFIXES.items():
        public_headers = collect_component_files(prefixes[0])
        implementation_files = collect_component_files(prefixes[1])
        inventory[component] = {
            "include": public_headers,
            "src": implementation_files,
        }
    return inventory


def collect_component_files(prefix: str) -> List[pathlib.Path]:
    root = ROOT / prefix
    if not root.exists():
        return []
    files = [
        path
        for path in root.rglob("*")
        if path.is_file() and path.suffix in CXX_SUFFIXES
    ]
    return sorted(files)


def add_component_edge(
    graph: Dict[str, Set[str]], src_component: Optional[str], dst_component: Optional[str]
) -> None:
    if src_component is None or dst_component is None or src_component == dst_component:
        return
    graph.setdefault(src_component, set()).add(dst_component)


def has_cross_component_internal_access(
    src: pathlib.Path,
    resolved: pathlib.Path,
    src_component: Optional[str],
    dst_component: Optional[str],
) -> bool:
    if dst_component is None or classify(resolved) != "src/lib":
        return False
    return not (classify(src) == "src/lib" and src_component == dst_component)


def detect_component_cycles(graph: Dict[str, Set[str]]) -> List[List[str]]:
    visited: Set[str] = set()
    stack: List[str] = []
    stack_positions: Dict[str, int] = {}
    found_cycles: Set[tuple[str, ...]] = set()

    def dfs(node: str) -> None:
        visited.add(node)
        stack_positions[node] = len(stack)
        stack.append(node)

        for neighbor in sorted(graph.get(node, set())):
            if neighbor not in visited:
                dfs(neighbor)
                continue
            if neighbor not in stack_positions:
                continue
            cycle = stack[stack_positions[neighbor] :] + [neighbor]
            found_cycles.add(canonicalize_cycle(cycle[:-1]))

        stack.pop()
        del stack_positions[node]

    for node in sorted(graph):
        if node not in visited:
            dfs(node)

    return [list(cycle) for cycle in sorted(found_cycles)]


def canonicalize_cycle(cycle: List[str]) -> tuple[str, ...]:
    rotations = [tuple(cycle[index:] + cycle[:index]) for index in range(len(cycle))]
    reversed_cycle = list(reversed(cycle))
    rotations.extend(
        tuple(reversed_cycle[index:] + reversed_cycle[:index])
        for index in range(len(reversed_cycle))
    )
    return min(rotations)


def add_component_orphan_checks(
    problems: List[str],
    inventory: Dict[str, Dict[str, List[pathlib.Path]]],
    architecture_items: Dict[str, Dict[str, str]],
    verified_architecture_ids: Set[str],
) -> None:
    realized_components = {
        component
        for component, files in inventory.items()
        if files["include"] or files["src"]
    }

    for component in sorted(realized_components):
        public_headers = inventory[component]["include"]
        architecture_id = COMPONENT_ARCHITECTURE.get(component)
        if architecture_id is None:
            problems.append(
                f"Orphan component: {component} has code but no owning architecture item mapping"
            )
            continue

        architecture_fields = architecture_items.get(architecture_id)
        if architecture_fields is None:
            problems.append(
                f"Orphan component: {component} has code but {architecture_id} is missing from ARCHITECTURE.md"
            )
            continue

        status = architecture_fields.get("Status", "").strip()
        if status == "OPEN":
            problems.append(
                f"Orphan component: {component} has code but owning architecture item {architecture_id} is OPEN"
            )
        if not public_headers:
            problems.append(
                f"Orphan component: {component} has implementation files but no public headers under include/project/{component}"
            )
        if architecture_id not in verified_architecture_ids:
            problems.append(
                f"Orphan component evidence: {component} ({architecture_id}) has no non-aux test @verifies coverage"
            )

    for component, architecture_id in sorted(COMPONENT_ARCHITECTURE.items()):
        architecture_fields = architecture_items.get(architecture_id)
        if architecture_fields is None:
            problems.append(
                f"Orphan architecture mapping: {component} points to missing item {architecture_id}"
            )
            continue

        status = architecture_fields.get("Status", "").strip()
        if status not in {"IN_PROGRESS", "DONE"}:
            continue
        if component not in realized_components:
            problems.append(
                f"Orphan architecture item: {architecture_id} ({component}) is {status} but has no component files"
            )


def main() -> int:
    allowed = parse_rules("## Allowed dependencies")
    component_allowed = parse_rules("## Allowed component dependencies")
    architecture_items = parse_architecture_items()
    verified_architecture_ids = parse_verified_architecture_ids()
    inventory = component_inventory()
    problems: List[str] = []
    component_graph: Dict[str, Set[str]] = {
        component: set()
        for component, files in inventory.items()
        if files["include"] or files["src"]
    }

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
            add_component_edge(component_graph, src_component, dst_component)
            if has_cross_component_internal_access(
                src, resolved, src_component, dst_component
            ):
                problems.append(
                    "Internal component include violation: "
                    f"{src.relative_to(ROOT)} must not include "
                    f"{resolved.relative_to(ROOT)}; cross-component access must go "
                    "through include/project/... public headers"
                )

    for cycle in detect_component_cycles(component_graph):
        cycle_text = " -> ".join(cycle + [cycle[0]])
        problems.append(f"Component cycle detected: {cycle_text}")

    add_component_orphan_checks(
        problems, inventory, architecture_items, verified_architecture_ids
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
