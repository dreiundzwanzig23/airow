#!/usr/bin/env python3
"""Traceability and process integrity checker.

Sources:
- docs/process/REQUIREMENTS.md with R-### entries
- docs/process/ARCHITECTURE.md with A-### entries (Satisfies [R-...], optional
  Evidence Profile CODE|ASSET)
- include/ and src/ files with Doxygen @design D-### blocks
- tests/ with Doxygen @test UT/IT/QT blocks (optional @aux yes overlay)

Generates docs/process/TRACEABILITY.md
"""

from __future__ import annotations

import json
import pathlib
import re
import sys
from typing import Dict, List, Set

ROOT = pathlib.Path(__file__).resolve().parents[1]
DOCS = ROOT / "docs" / "process"
SRC_DIRS = [ROOT / "include", ROOT / "src"]
TEST_DIR = ROOT / "tests"

MD_ID = re.compile(r"^#{2,6}\s+(?P<id>(R|A)-\d{3})\b", re.M)
FIELD_START = re.compile(r"^- \*\*(?P<key>[^*]+)\*\*:\s*(?P<val>.*)$")

BLOCK = re.compile(r"/\*\*([\s\S]*?)\*/", re.M)
TAG = re.compile(
    r"@(?P<k>design|title|satisfies|refines|test|verifies|notes|aux)\s+(?P<v>.+)"
)
ID = re.compile(r"^(R|A|D|UT|IT|QT)-\d{3}$")
FUNCTION_DEF = re.compile(
    r"^\s*(?!if\b|for\b|while\b|switch\b|catch\b)"
    r"(?:[~\w:<>,\s*&]+?\s+)?(?P<name>[~A-Za-z_]\w*(?:::\w+)*)\s*"
    r"\([^;{}]*\)\s*(?:const\s*)?\{",
    re.M,
)

ALLOWED_STATUS = {"OPEN", "IN_PROGRESS", "DONE"}
ALLOWED_ARCH_EVIDENCE_PROFILES = {"CODE", "ASSET"}
ALLOWED_REQUIREMENT_CHANGE_TYPES = {"none", "editorial", "semantic"}
ALLOWED_REQUIREMENT_NEEDS_REVIEW = {"yes", "no"}
ALLOWED_AUX_TAG_VALUES = {"yes", "no", "true", "false", "1", "0"}
BOOTSTRAP_ID_MIN = 900
REQUIRED_REQUIREMENT_FIELDS = [
    "Title",
    "Acceptance Criteria",
    "Priority",
    "Status",
    "Created",
    "Updated",
    "Change-Type",
    "Needs-Review",
]
REQUIRED_ARCH_FIELDS = ["Title", "Satisfies", "Status"]
REQUIRED_ARCH_CODE_FIELDS = [
    "Responsibility",
    "Owned Concepts",
    "Inputs",
    "Outputs",
    "Depends On",
    "Must Not Depend On",
    "Invariants",
    "Allocation Rationale",
    "Future Absorption",
    "Interfaces",
]

REQUIRED_CONTEXT_FILES = [
    "docs/ai/SESSION_CONTEXT.md",
    "docs/ai/DECISIONS.md",
    "docs/ai/ROADMAP.md",
    "docs/ai/HANDOFF.md",
    "docs/ai/archive/README.md",
    "docs/ai/archive/DECISIONS_INDEX.md",
    "docs/process/WORKFLOW.md",
    "docs/process/TEST_STRATEGY.md",
    "docs/process/MAINTENANCE.md",
    "docs/process/ARCHITECTURE_POLICY.md",
    "docs/process/ARCHITECTURE_HEALTH.md",
    "docs/process/MODEL_FIDELITY.md",
    "docs/process/NUMERICS_POLICY.md",
    "docs/process/CALIBRATION_PROVENANCE.md",
    "docs/process/TECHNOLOGY_STACK.md",
    "docs/process/LLM_DRIFT_REVIEW.md",
    "skills/README.md",
]

REQUIRED_LOCAL_AGENTS = [
    "docs/process/AGENTS.md",
    "src/AGENTS.md",
    "include/AGENTS.md",
    "tests/AGENTS.md",
    "tests/unit/AGENTS.md",
    "tests/integration/AGENTS.md",
    "tests/system/AGENTS.md",
    "scripts/AGENTS.md",
    "tools/AGENTS.md",
]


def parse_list_field(value: str) -> List[str]:
    value = value.strip()
    if not (value.startswith("[") and value.endswith("]")):
        return []
    inner = value[1:-1].strip()
    if not inner:
        return []
    return [part.strip() for part in inner.split(",") if part.strip()]


def id_number(value: str) -> int:
    return int(value.split("-")[1])


def is_bootstrap_id(value: str) -> bool:
    return bool(re.match(ID, value)) and id_number(value) >= BOOTSTRAP_ID_MIN


def normalize_title(value: str) -> str:
    cleaned = re.sub(r"[^a-z0-9]+", " ", value.lower())
    return " ".join(cleaned.split())


def architecture_evidence_profile(fields: Dict[str, str]) -> str:
    profile = fields.get("Evidence Profile", "").strip().upper()
    return profile if profile else "CODE"


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


def parse_sections(path: pathlib.Path, prefix: str) -> Dict[str, Dict[str, str]]:
    text = path.read_text(encoding="utf-8") if path.exists() else ""
    items: Dict[str, Dict[str, str]] = {}
    matches = [
        match for match in re.finditer(MD_ID, text) if match.group("id").startswith(prefix)
    ]
    for index, match in enumerate(matches):
        item_id = match.group("id")
        start = match.end()
        end = matches[index + 1].start() if index + 1 < len(matches) else len(text)
        block = text[start : end if end != -1 else None]
        items[item_id] = parse_markdown_fields(block)
    return items


def find_code_files(root: pathlib.Path):
    for path in root.rglob("*"):
        if path.is_file() and path.suffix in (".hpp", ".h", ".ipp", ".cpp", ".cc", ".cxx"):
            yield path


def parse_design_from_code() -> Dict[str, Dict[str, object]]:
    data: Dict[str, Dict[str, object]] = {}
    for base in SRC_DIRS:
        if not base.exists():
            continue
        for file_path in find_code_files(base):
            text = file_path.read_text(encoding="utf-8", errors="ignore")
            for block_match in re.finditer(BLOCK, text):
                tags = {
                    tag_match.group("k"): tag_match.group("v").strip()
                    for tag_match in re.finditer(TAG, block_match.group(1))
                }
                if "design" not in tags:
                    continue
                design_id = tags["design"].split()[0]
                if not re.match(ID, design_id) or not design_id.startswith("D-"):
                    continue

                title = tags.get("title", "")
                satisfies = parse_list_field(tags.get("satisfies", "[]"))
                refines = parse_list_field(tags.get("refines", "[]"))
                rel_path = str(file_path.relative_to(ROOT))
                line_no = _line_no(text, block_match.start())

                entry = data.setdefault(
                    design_id,
                    {
                        "Title": title,
                        "Satisfies": set(),
                        "Refines": set(),
                        "Files": set(),
                        "Occurrences": [],
                    },
                )
                if title and not entry["Title"]:
                    entry["Title"] = title
                entry["Satisfies"].update(satisfies)
                entry["Refines"].update(refines)
                entry["Files"].add(rel_path)
                entry["Occurrences"].append({"File": rel_path, "Line": line_no})

    for value in data.values():
        value["Satisfies"] = sorted(value["Satisfies"])
        value["Refines"] = sorted(value["Refines"])
        value["Files"] = sorted(value["Files"])
        value["Occurrences"] = sorted(
            value["Occurrences"], key=lambda occ: (occ["File"], occ["Line"])
        )
    return data


def _line_no(text: str, pos: int) -> int:
    return text.count("\n", 0, pos) + 1


def _find_block_end_lines_with_design(text: str) -> List[int]:
    lines: List[int] = []
    for block_match in re.finditer(BLOCK, text):
        tags = {
            tag_match.group("k"): tag_match.group("v").strip()
            for tag_match in re.finditer(TAG, block_match.group(1))
        }
        if "design" in tags:
            lines.append(_line_no(text, block_match.end()))
    return lines


def _find_matching_brace(text: str, open_index: int) -> int:
    depth = 0
    for idx in range(open_index, len(text)):
        char = text[idx]
        if char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0:
                return idx
    return -1


def _is_nontrivial_function(body: str) -> bool:
    if re.search(r"\b(if|for|while|switch|do)\b", body):
        return True
    semicolon_count = body.count(";")
    non_blank_lines = [
        line for line in body.splitlines() if line.strip() and line.strip() not in {"{", "}"}
    ]
    return semicolon_count > 1 or len(non_blank_lines) > 2


def find_unmapped_nontrivial_functions() -> List[str]:
    problems: List[str] = []
    scan_root = ROOT / "src" / "lib"
    if not scan_root.exists():
        return problems

    for file_path in find_code_files(scan_root):
        text = file_path.read_text(encoding="utf-8", errors="ignore")
        rel = str(file_path.relative_to(ROOT))
        design_end_lines = _find_block_end_lines_with_design(text)

        for match in re.finditer(FUNCTION_DEF, text):
            name = match.group("name")
            if name in {"if", "for", "while", "switch", "catch"}:
                continue
            signature_line = _line_no(text, match.start())

            line_start = text.rfind("\n", 0, match.start()) + 1
            line_end = text.find("\n", match.start())
            if line_end == -1:
                line_end = len(text)
            signature_line_text = text[line_start:line_end]
            if "trace: trivial" in signature_line_text:
                continue

            brace_index = match.end() - 1
            close_index = _find_matching_brace(text, brace_index)
            if close_index == -1:
                continue
            body = text[brace_index : close_index + 1]

            if not _is_nontrivial_function(body):
                continue

            mapped = any(0 <= signature_line - block_end <= 12 for block_end in design_end_lines)
            if not mapped:
                problems.append(
                    f"{rel}:{signature_line} non-trivial function '{name}' has no @design D-### mapping"
                )

    return problems


def parse_tests() -> Dict[str, Dict[str, Dict[str, object]]]:
    tests: Dict[str, Dict[str, Dict[str, object]]] = {"UT": {}, "IT": {}, "QT": {}}
    if not TEST_DIR.exists():
        return tests

    for file_path in TEST_DIR.rglob("*.cpp"):
        text = file_path.read_text(encoding="utf-8", errors="ignore")
        for block_match in re.finditer(BLOCK, text):
            tags = {
                tag_match.group("k"): tag_match.group("v").strip()
                for tag_match in re.finditer(TAG, block_match.group(1))
            }
            if "test" not in tags:
                continue

            test_id = tags["test"].split()[0]
            if not re.match(ID, test_id):
                continue
            kind = test_id.split("-")[0]
            if kind not in tests:
                continue

            verifies = parse_list_field(tags.get("verifies", "[]"))
            aux_tag_raw = tags.get("aux", "").strip().lower()
            is_aux = aux_tag_raw in {"yes", "true", "1"}
            rel_path = str(file_path.relative_to(ROOT))
            line_no = _line_no(text, block_match.start())
            entry = tests[kind].setdefault(
                test_id,
                {
                    "Verifies": set(),
                    "Files": set(),
                    "Occurrences": [],
                    "Aux": False,
                    "AuxTagValues": set(),
                },
            )
            entry["Verifies"].update(verifies)
            entry["Files"].add(rel_path)
            entry["Occurrences"].append({"File": rel_path, "Line": line_no})
            entry["Aux"] = bool(entry["Aux"] or is_aux)
            if aux_tag_raw:
                entry["AuxTagValues"].add(aux_tag_raw)

    for kind in ("UT", "IT", "QT"):
        for entry in tests[kind].values():
            entry["Verifies"] = sorted(entry["Verifies"])
            entry["Files"] = sorted(entry["Files"])
            entry["Occurrences"] = sorted(
                entry["Occurrences"], key=lambda occ: (occ["File"], occ["Line"])
            )
            entry["AuxTagValues"] = sorted(entry["AuxTagValues"])
            entry["File"] = entry["Files"][0]

    return tests


def validate_required_files(problems: List[str]) -> None:
    for rel_path in REQUIRED_CONTEXT_FILES + REQUIRED_LOCAL_AGENTS:
        if not (ROOT / rel_path).exists():
            problems.append(f"Required process file missing: {rel_path}")


def validate_metadata_fields(
    requirements: Dict[str, Dict[str, str]],
    architecture: Dict[str, Dict[str, str]],
    problems: List[str],
) -> None:
    for req_id, fields in requirements.items():
        for field in REQUIRED_REQUIREMENT_FIELDS:
            if field not in fields or not fields[field].strip():
                problems.append(f"{req_id} missing required field '{field}'")
        status = fields.get("Status", "")
        if status and status not in ALLOWED_STATUS:
            problems.append(f"{req_id} has invalid Status '{status}'")
        change_type = fields.get("Change-Type", "").strip().lower()
        if change_type and change_type not in ALLOWED_REQUIREMENT_CHANGE_TYPES:
            problems.append(f"{req_id} has invalid Change-Type '{fields.get('Change-Type', '')}'")
        needs_review = fields.get("Needs-Review", "").strip().lower()
        if needs_review and needs_review not in ALLOWED_REQUIREMENT_NEEDS_REVIEW:
            problems.append(
                f"{req_id} has invalid Needs-Review '{fields.get('Needs-Review', '')}'"
            )
        if change_type == "semantic" and needs_review and needs_review != "yes":
            problems.append(
                f"{req_id} has semantic Change-Type but Needs-Review is not 'yes'"
            )

    for arch_id, fields in architecture.items():
        for field in REQUIRED_ARCH_FIELDS:
            if field not in fields or not fields[field].strip():
                problems.append(f"{arch_id} missing required field '{field}'")
        status = fields.get("Status", "")
        if status and status not in ALLOWED_STATUS:
            problems.append(f"{arch_id} has invalid Status '{status}'")
        profile = architecture_evidence_profile(fields)
        if profile not in ALLOWED_ARCH_EVIDENCE_PROFILES:
            problems.append(
                f"{arch_id} has invalid Evidence Profile '{fields.get('Evidence Profile', '')}'"
            )
        if profile == "CODE":
            for field in REQUIRED_ARCH_CODE_FIELDS:
                if field not in fields or not fields[field].strip():
                    problems.append(f"{arch_id} missing required CODE field '{field}'")


def validate_model(data: Dict[str, object]) -> List[str]:
    problems: List[str] = []
    validate_required_files(problems)

    requirements: Dict[str, Dict[str, str]] = data["R"]
    architecture: Dict[str, Dict[str, str]] = data["A"]
    design: Dict[str, Dict[str, object]] = data["D"]

    validate_metadata_fields(requirements, architecture, problems)

    for design_id, fields in design.items():
        occurrences = fields.get("Occurrences", [])
        if len(occurrences) > 1:
            locations = ", ".join(
                f"{occ['File']}:{occ['Line']}" for occ in occurrences
            )
            problems.append(
                f"{design_id} is declared in multiple @design blocks: {locations}"
            )

    for test_kind in ("UT", "IT", "QT"):
        for test_id, fields in data[test_kind].items():
            occurrences = fields.get("Occurrences", [])
            if len(occurrences) > 1:
                locations = ", ".join(
                    f"{occ['File']}:{occ['Line']}" for occ in occurrences
                )
                problems.append(f"{test_id} is declared multiple times: {locations}")

    a_by_r: Dict[str, List[str]] = {req_id: [] for req_id in requirements}
    for arch_id, fields in architecture.items():
        satisfies = parse_list_field(fields.get("Satisfies", "[]"))
        for req_id in satisfies:
            a_by_r.setdefault(req_id, []).append(arch_id)

    d_by_a: Dict[str, List[str]] = {}
    for design_id, fields in design.items():
        for arch_id in fields.get("Satisfies", []):
            d_by_a.setdefault(arch_id, []).append(design_id)

    ut_by_d: Dict[str, List[str]] = {}
    for test_id, fields in data["UT"].items():
        if fields.get("Aux", False):
            continue
        for ref in fields.get("Verifies", []):
            if ref.startswith("D-"):
                ut_by_d.setdefault(ref, []).append(test_id)

    it_by_a: Dict[str, List[str]] = {}
    for test_id, fields in data["IT"].items():
        if fields.get("Aux", False):
            continue
        for ref in fields.get("Verifies", []):
            if ref.startswith("A-"):
                it_by_a.setdefault(ref, []).append(test_id)

    qt_by_r: Dict[str, List[str]] = {}
    for test_id, fields in data["QT"].items():
        if fields.get("Aux", False):
            continue
        for ref in fields.get("Verifies", []):
            if ref.startswith("R-"):
                qt_by_r.setdefault(ref, []).append(test_id)

    for req_id, fields in requirements.items():
        status = fields.get("Status", "OPEN")
        if status in {"DONE", "IN_PROGRESS"} and not a_by_r.get(req_id):
            problems.append(f"{req_id} ({status}) is not satisfied by any A-###")
        if status == "DONE" and not qt_by_r.get(req_id):
            problems.append(f"{req_id} (DONE) has no qualifying QT-### test")

    for arch_id, fields in architecture.items():
        status = fields.get("Status", "OPEN")
        profile = architecture_evidence_profile(fields)
        if profile == "CODE":
            if status in {"DONE", "IN_PROGRESS"} and not d_by_a.get(arch_id):
                problems.append(f"{arch_id} ({status}) is not implemented by any D-###")
            if status == "DONE" and not it_by_a.get(arch_id):
                problems.append(f"{arch_id} (DONE) has no qualifying IT-### test")
            continue

        if status == "DONE":
            req_ids = parse_list_field(fields.get("Satisfies", "[]"))
            qt_matches: Set[str] = set()
            for req_id in req_ids:
                qt_matches.update(qt_by_r.get(req_id, []))
            if not qt_matches:
                problems.append(
                    f"{arch_id} (DONE, ASSET) has no qualifying QT-### test via satisfied R-###"
                )

    for design_id, fields in design.items():
        if not fields.get("Files"):
            problems.append(f"{design_id} has no implementing files")
        if not ut_by_d.get(design_id):
            problems.append(f"{design_id} has no qualifying UT-### test")

    all_ids: Set[str] = set(requirements.keys()) | set(architecture.keys()) | set(design.keys())
    all_ids |= set(data["UT"].keys()) | set(data["IT"].keys()) | set(data["QT"].keys())

    for arch_id, fields in architecture.items():
        for req_id in parse_list_field(fields.get("Satisfies", "[]")):
            if req_id not in requirements:
                problems.append(f"{arch_id} satisfies unknown requirement {req_id}")

    for design_id, fields in design.items():
        for arch_id in fields.get("Satisfies", []):
            if arch_id not in architecture:
                problems.append(f"{design_id} satisfies unknown architecture item {arch_id}")
        for refined_design_id in fields.get("Refines", []):
            if refined_design_id not in design:
                problems.append(f"{design_id} refines unknown design item {refined_design_id}")
            elif not refined_design_id.startswith("D-"):
                problems.append(
                    f"{design_id} refines invalid item {refined_design_id}; expected D-###"
                )

    for test_kind in ("UT", "IT", "QT"):
        for test_id, fields in data[test_kind].items():
            aux_tag_values = fields.get("AuxTagValues", [])
            for aux_value in aux_tag_values:
                if aux_value not in ALLOWED_AUX_TAG_VALUES:
                    problems.append(
                        f"{test_id} has invalid @aux value '{aux_value}' (expected yes|no)"
                    )

            for ref in fields.get("Verifies", []):
                if ref not in all_ids:
                    problems.append(f"{test_id} references missing ID {ref}")

            if fields.get("Aux", False):
                continue

            if test_kind == "UT" and any(
                not ref.startswith("D-") for ref in fields.get("Verifies", [])
            ):
                problems.append(f"{test_id} in UT must verify only D-### IDs")
            if test_kind == "UT":
                d_refs = [ref for ref in fields.get("Verifies", []) if ref.startswith("D-")]
                if not d_refs:
                    problems.append(f"{test_id} in UT must verify at least one D-### ID")

            if test_kind == "IT" and any(
                not ref.startswith("A-") for ref in fields.get("Verifies", [])
            ):
                problems.append(f"{test_id} in IT must verify only A-### IDs")
            if test_kind == "IT":
                a_refs = [ref for ref in fields.get("Verifies", []) if ref.startswith("A-")]
                if not a_refs:
                    problems.append(f"{test_id} in IT must verify at least one A-### ID")

            if test_kind == "QT" and any(
                not ref.startswith("R-") for ref in fields.get("Verifies", [])
            ):
                problems.append(f"{test_id} in QT must verify only R-### IDs")
            if test_kind == "QT":
                r_refs = [ref for ref in fields.get("Verifies", []) if ref.startswith("R-")]
                if not r_refs:
                    problems.append(f"{test_id} in QT must verify at least one R-### ID")

    problems.extend(find_unmapped_nontrivial_functions())

    return problems


def numbering_warnings(data: Dict[str, object]) -> List[str]:
    warnings: List[str] = []

    def format_missing_ranges(prefix: str, missing_nums: List[int]) -> str:
        if not missing_nums:
            return ""
        ranges: List[tuple[int, int]] = []
        start = missing_nums[0]
        end = start
        for value in missing_nums[1:]:
            if value == end + 1:
                end = value
                continue
            ranges.append((start, end))
            start = value
            end = value
        ranges.append((start, end))

        parts: List[str] = []
        for begin, finish in ranges:
            if begin == finish:
                parts.append(f"{prefix}-{begin:03d}")
            else:
                parts.append(f"{prefix}-{begin:03d}..{prefix}-{finish:03d}")
        return ", ".join(parts)

    def check(ids: Set[str], prefix: str) -> None:
        if not ids:
            return
        nums = sorted(
            int(item.split("-")[1])
            for item in ids
            if item.startswith(f"{prefix}-") and not is_bootstrap_id(item)
        )
        if not nums:
            return
        expected = set(range(1, max(nums) + 1))
        missing = sorted(expected.difference(nums))
        if missing:
            missing_str = format_missing_ranges(prefix, missing)
            warnings.append(f"{prefix} numbering has gaps; missing IDs: {missing_str}")

    check(set(data["R"].keys()), "R")
    check(set(data["A"].keys()), "A")
    check(set(data["D"].keys()), "D")
    check(set(data["UT"].keys()), "UT")
    check(set(data["IT"].keys()), "IT")
    check(set(data["QT"].keys()), "QT")
    return warnings


def architecture_warnings(data: Dict[str, object]) -> List[str]:
    warnings: List[str] = []
    requirements: Dict[str, Dict[str, str]] = data["R"]
    architecture: Dict[str, Dict[str, str]] = data["A"]
    design: Dict[str, Dict[str, object]] = data["D"]

    a_by_r: Dict[str, List[str]] = {}
    for arch_id, fields in architecture.items():
        if is_bootstrap_id(arch_id):
            continue
        for req_id in parse_list_field(fields.get("Satisfies", "[]")):
            if is_bootstrap_id(req_id):
                continue
            a_by_r.setdefault(req_id, []).append(arch_id)

    d_by_a: Dict[str, List[str]] = {}
    for design_id, fields in design.items():
        if is_bootstrap_id(design_id):
            continue
        for arch_id in fields.get("Satisfies", []):
            if is_bootstrap_id(arch_id):
                continue
            d_by_a.setdefault(arch_id, []).append(design_id)

    it_by_a: Dict[str, List[str]] = {}
    for test_id, fields in data["IT"].items():
        if fields.get("Aux", False) or is_bootstrap_id(test_id):
            continue
        for ref in fields.get("Verifies", []):
            if ref.startswith("A-") and not is_bootstrap_id(ref):
                it_by_a.setdefault(ref, []).append(test_id)

    normalized_arch_titles: Dict[str, List[str]] = {}
    for arch_id, fields in architecture.items():
        if is_bootstrap_id(arch_id):
            continue
        title = fields.get("Title", "")
        normalized = normalize_title(title)
        if normalized:
            normalized_arch_titles.setdefault(normalized, []).append(arch_id)
        for req_id in parse_list_field(fields.get("Satisfies", "[]")):
            if is_bootstrap_id(req_id):
                continue
            if normalize_title(title) == normalize_title(requirements.get(req_id, {}).get("Title", "")):
                warnings.append(
                    f"{arch_id} title exactly matches satisfied requirement {req_id}; review for requirement mirroring"
                )

    for normalized, arch_ids in sorted(normalized_arch_titles.items()):
        if len(arch_ids) > 1:
            warnings.append(
                f"Architecture items share the same normalized title '{normalized}': {', '.join(sorted(arch_ids))}"
            )

    mapped_requirements = sorted(a_by_r.keys())
    if len(mapped_requirements) >= 5:
        one_to_one_count = sum(1 for req_id in mapped_requirements if len(set(a_by_r[req_id])) == 1)
        ratio = one_to_one_count / len(mapped_requirements)
        if ratio > 0.80:
            warnings.append(
                "More than 80% of mapped non-bootstrap requirements allocate to exactly one architecture item; review for fragmentation"
            )

    for arch_id, fields in architecture.items():
        if is_bootstrap_id(arch_id):
            continue
        if architecture_evidence_profile(fields) != "CODE":
            continue
        req_ids = [
            req_id for req_id in parse_list_field(fields.get("Satisfies", "[]"))
            if not is_bootstrap_id(req_id)
        ]
        design_ids = d_by_a.get(arch_id, [])
        it_ids = it_by_a.get(arch_id, [])
        future_absorption = fields.get("Future Absorption", "").strip()
        if len(req_ids) == 1 and len(design_ids) == 1 and len(it_ids) == 1 and not future_absorption:
            warnings.append(
                f"{arch_id} has a narrow 1R/1D/1IT footprint and empty Future Absorption; review for thin architecture ownership"
            )

    return warnings


def write_traceability(data: Dict[str, object], warnings: List[str]) -> None:
    requirements: Dict[str, Dict[str, str]] = data["R"]
    architecture: Dict[str, Dict[str, str]] = data["A"]
    design: Dict[str, Dict[str, object]] = data["D"]

    a_by_r: Dict[str, List[str]] = {}
    for arch_id, fields in architecture.items():
        for req_id in parse_list_field(fields.get("Satisfies", "[]")):
            a_by_r.setdefault(req_id, []).append(arch_id)

    d_by_a: Dict[str, List[str]] = {}
    for design_id, fields in design.items():
        for arch_id in fields.get("Satisfies", []):
            d_by_a.setdefault(arch_id, []).append(design_id)

    lines = [
        "<!-- Generated by tools/tracecheck.py. Do not hand-edit. -->",
        "",
        "| Requirement | Req Status | Satisfied by A | Arch Status | Arch Evidence | Implemented by D | Unit Tests (UT) | Integration (IT) | System (QT) |",
        "|-------------|------------|----------------|-------------|---------------|------------------|-----------------|------------------|-------------|",
    ]

    for req_id in sorted(requirements.keys()):
        req_status = requirements[req_id].get("Status", "OPEN")
        arch_ids = sorted(a_by_r.get(req_id, []))
        if not arch_ids:
            qt_tests = sorted(
                [
                    test_id
                    for test_id, fields in data["QT"].items()
                    if req_id in fields.get("Verifies", []) and not fields.get("Aux", False)
                ]
            )
            lines.append(
                f"| {req_id} | {req_status} | — | — | — | — | — | — | {', '.join(qt_tests) or '—'} |"
            )
            continue

        for arch_id in arch_ids:
            arch_status = architecture.get(arch_id, {}).get("Status", "OPEN")
            arch_profile = architecture_evidence_profile(architecture.get(arch_id, {}))
            design_ids = sorted(d_by_a.get(arch_id, []))
            if not design_ids:
                it_tests = sorted(
                    [
                        test_id
                        for test_id, fields in data["IT"].items()
                        if arch_id in fields.get("Verifies", []) and not fields.get("Aux", False)
                    ]
                )
                qt_tests = sorted(
                    [
                        test_id
                        for test_id, fields in data["QT"].items()
                        if req_id in fields.get("Verifies", []) and not fields.get("Aux", False)
                    ]
                )
                lines.append(
                    f"| {req_id} | {req_status} | {arch_id} | {arch_status} | {arch_profile} | — | — | {', '.join(it_tests) or '—'} | {', '.join(qt_tests) or '—'} |"
                )
                continue

            for design_id in design_ids:
                ut_tests = sorted(
                    [
                        test_id
                        for test_id, fields in data["UT"].items()
                        if design_id in fields.get("Verifies", []) and not fields.get("Aux", False)
                    ]
                )
                it_tests = sorted(
                    [
                        test_id
                        for test_id, fields in data["IT"].items()
                        if arch_id in fields.get("Verifies", []) and not fields.get("Aux", False)
                    ]
                )
                qt_tests = sorted(
                    [
                        test_id
                        for test_id, fields in data["QT"].items()
                        if req_id in fields.get("Verifies", []) and not fields.get("Aux", False)
                    ]
                )
                lines.append(
                    f"| {req_id} | {req_status} | {arch_id} | {arch_status} | {arch_profile} | {design_id} | {', '.join(ut_tests) or '—'} | {', '.join(it_tests) or '—'} | {', '.join(qt_tests) or '—'} |"
                )

    design_refines_rows: List[str] = []
    for design_id in sorted(design.keys()):
        for parent in sorted(design[design_id].get("Refines", [])):
            design_refines_rows.append(f"| {design_id} | {parent} |")

    lines.extend(
        [
            "",
            "## Design Refinement Links",
            "",
            "| Design | Refines Design |",
            "|--------|----------------|",
        ]
    )
    lines.extend(design_refines_rows or ["| — | — |"])

    lines.extend(
        [
            "",
            "## Architecture Details",
            "",
        ]
    )
    for arch_id in sorted(architecture.keys()):
        fields = architecture[arch_id]
        lines.extend(
            [
                f"### {arch_id} — {fields.get('Title', 'Untitled')}",
                f"- **Status**: {fields.get('Status', 'OPEN')}",
                f"- **Evidence Profile**: {architecture_evidence_profile(fields)}",
                f"- **Satisfies**: {fields.get('Satisfies', '[]')}",
                f"- **Responsibility**: {fields.get('Responsibility', '—')}",
                f"- **Owned Concepts**: {fields.get('Owned Concepts', '—')}",
                f"- **Inputs**: {fields.get('Inputs', '—')}",
                f"- **Outputs**: {fields.get('Outputs', '—')}",
                f"- **Depends On**: {fields.get('Depends On', '—')}",
                f"- **Must Not Depend On**: {fields.get('Must Not Depend On', '—')}",
                f"- **Invariants**: {fields.get('Invariants', '—')}",
                f"- **Allocation Rationale**: {fields.get('Allocation Rationale', '—')}",
                f"- **Future Absorption**: {fields.get('Future Absorption', '—')}",
                f"- **Interfaces**: {fields.get('Interfaces', '—')}",
                "",
            ]
        )

    lines.extend(
        [
            "## Architecture Warnings",
            "",
        ]
    )
    if warnings:
        lines.extend([f"- {warning}" for warning in warnings])
    else:
        lines.append("- None.")

    aux_rows: List[str] = []
    for test_kind in ("UT", "IT", "QT"):
        for test_id in sorted(data[test_kind].keys()):
            fields = data[test_kind][test_id]
            if not fields.get("Aux", False):
                continue
            verifies = ", ".join(fields.get("Verifies", [])) or "—"
            file_path = fields.get("File", "—")
            aux_rows.append(f"| {test_id} | {test_kind} | {verifies} | {file_path} |")

    lines.extend(
        [
            "",
            "## Auxiliary Tests (Excluded from Evidence Gates)",
            "",
            "| Test | Lane | Verifies (Informational) | File |",
            "|------|------|---------------------------|------|",
        ]
    )
    lines.extend(aux_rows or ["| — | — | — | — |"])

    (DOCS / "TRACEABILITY.md").write_text("\n".join(lines), encoding="utf-8")


def collect_data() -> Dict[str, object]:
    requirements = parse_sections(DOCS / "REQUIREMENTS.md", "R-")
    architecture = parse_sections(DOCS / "ARCHITECTURE.md", "A-")
    design = parse_design_from_code()
    tests = parse_tests()
    return {"R": requirements, "A": architecture, "D": design, **tests}


def main(argv: List[str]) -> int:
    data = collect_data()
    problems = validate_model(data)
    warnings = numbering_warnings(data) + architecture_warnings(data)

    if "--write" in argv:
        write_traceability(data, warnings)

    if "--json" in argv:
        print(json.dumps({"problems": problems, "warnings": warnings, "data": data}, indent=2))

    if problems:
        print("TRACE CHECK FAILED:")
        for problem in problems:
            print(f" - {problem}")
        return 1

    if warnings:
        print("TRACE CHECK WARNING:")
        for warning in warnings:
            print(f" - {warning}")

    print("Trace OK")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
