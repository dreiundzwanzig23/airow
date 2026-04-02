#!/usr/bin/env python3
"""Lightweight consistency checks for agent/process instruction docs."""

from __future__ import annotations

import pathlib
import re
import sys
from typing import List, Set

ROOT = pathlib.Path(__file__).resolve().parents[1]
DOCS = [
    ROOT / "AGENTS.md",
    ROOT / "README.md",
    ROOT / "docs" / "process" / "ARCHITECTURE_POLICY.md",
    ROOT / "docs" / "process" / "ARCHITECTURE_HEALTH.md",
    ROOT / "docs" / "process" / "MODEL_FIDELITY.md",
    ROOT / "docs" / "process" / "NUMERICS_POLICY.md",
    ROOT / "docs" / "process" / "CALIBRATION_PROVENANCE.md",
    ROOT / "docs" / "process" / "WORKFLOW.md",
    ROOT / "docs" / "process" / "TEST_STRATEGY.md",
    ROOT / "docs" / "process" / "TECHNOLOGY_STACK.md",
]
AI_CONTEXT_BUDGETS = {
    ROOT / "docs" / "ai" / "SESSION_CONTEXT.md": 70,
    ROOT / "docs" / "ai" / "HANDOFF.md": 60,
    ROOT / "docs" / "ai" / "ROADMAP.md": 70,
}
REQUIRED_SKILLS = [
    ROOT / ".agents" / "skills" / "tdd-loop" / "SKILL.md",
    ROOT / ".agents" / "skills" / "major-change-loop" / "SKILL.md",
    ROOT / ".agents" / "skills" / "trace-maintenance" / "SKILL.md",
    ROOT / ".agents" / "skills" / "test-lanes" / "SKILL.md",
    ROOT / ".agents" / "skills" / "release-doc-sync" / "SKILL.md",
]
REQUIRED_REFERENCES = {
    ROOT / "AGENTS.md": [
        "docs/process/ARCHITECTURE_POLICY.md",
        "docs/process/TECHNOLOGY_STACK.md",
        "docs/ai/DECISIONS.md",
        ".agents/skills/major-change-loop/SKILL.md",
    ],
    ROOT / "README.md": [
        "docs/process/ARCHITECTURE_POLICY.md",
        "docs/process/TECHNOLOGY_STACK.md",
        "docs/ai/DECISIONS.md",
        ".agents/skills/major-change-loop/SKILL.md",
    ],
    ROOT / "docs" / "process" / "WORKFLOW.md": [
        "docs/process/ARCHITECTURE_POLICY.md",
        "docs/process/TECHNOLOGY_STACK.md",
        "docs/ai/DECISIONS.md",
        ".agents/skills/major-change-loop/SKILL.md",
    ],
}
SCRIPT_REF_RE = re.compile(r"\./scripts/[A-Za-z0-9_./-]+\.sh")
SKILL_REF_RE = re.compile(r"\.agents/skills/[A-Za-z0-9_-]+/SKILL\.md")
BANNED_PHRASES = [
    "must verify exactly one D-### ID",
    "must verify exactly one same-layer item",
]


def load_max_active_adrs() -> int:
    archiver = ROOT / "tools" / "archive_ai_decisions.py"
    text = archiver.read_text(encoding="utf-8", errors="ignore")
    match = re.search(r"DEFAULT_MAX_ACTIVE_ADRS\s*=\s*(\d+)", text)
    if not match:
        raise RuntimeError(
            "Could not determine DEFAULT_MAX_ACTIVE_ADRS from tools/archive_ai_decisions.py"
        )
    return int(match.group(1))


def check_docs_exist(problems: List[str]) -> None:
    for doc in DOCS:
        if not doc.exists():
            problems.append(f"Missing instruction doc: {doc.relative_to(ROOT)}")


def check_skill_files(problems: List[str]) -> None:
    for skill_path in REQUIRED_SKILLS:
        if not skill_path.exists():
            problems.append(f"Missing required skill: {skill_path.relative_to(ROOT)}")


def check_references(problems: List[str]) -> None:
    referenced_skills: Set[str] = set()

    for doc in DOCS:
        text = doc.read_text(encoding="utf-8", errors="ignore")

        for phrase in BANNED_PHRASES:
            if phrase in text:
                problems.append(
                    f"{doc.relative_to(ROOT)} contains banned phrase: '{phrase}'"
                )

        for script_ref in SCRIPT_REF_RE.findall(text):
            candidate = ROOT / script_ref[2:]
            if not candidate.exists():
                problems.append(
                    f"{doc.relative_to(ROOT)} references missing script: {script_ref}"
                )

        for skill_ref in SKILL_REF_RE.findall(text):
            candidate = ROOT / skill_ref
            referenced_skills.add(skill_ref)
            if not candidate.exists():
                problems.append(
                    f"{doc.relative_to(ROOT)} references missing skill: {skill_ref}"
                )

        for required_ref in REQUIRED_REFERENCES.get(doc, []):
            if required_ref not in text:
                problems.append(
                    f"{doc.relative_to(ROOT)} must reference {required_ref}"
                )

    for skill_path in REQUIRED_SKILLS:
        rel = skill_path.relative_to(ROOT).as_posix()
        if rel not in referenced_skills:
            problems.append(f"Required skill not referenced by core docs: {rel}")


def check_ai_context_budgets(problems: List[str]) -> None:
    for path, max_lines in AI_CONTEXT_BUDGETS.items():
        if not path.exists():
            problems.append(f"Missing AI context doc: {path.relative_to(ROOT)}")
            continue
        line_count = len(path.read_text(encoding="utf-8", errors="ignore").splitlines())
        if line_count > max_lines:
            problems.append(
                f"{path.relative_to(ROOT)} exceeds active-doc budget: "
                f"{line_count} lines > {max_lines}"
            )


def check_active_decision_budget(problems: List[str]) -> None:
    decisions = ROOT / "docs" / "ai" / "DECISIONS.md"
    if not decisions.exists():
        problems.append(f"Missing AI decisions doc: {decisions.relative_to(ROOT)}")
        return

    max_active_adrs = load_max_active_adrs()
    text = decisions.read_text(encoding="utf-8", errors="ignore")
    adr_count = len(re.findall(r"^## ADR-", text, flags=re.MULTILINE))
    if adr_count > max_active_adrs:
        problems.append(
            f"{decisions.relative_to(ROOT)} exceeds active ADR budget: "
            f"{adr_count} > {max_active_adrs}"
        )


def main() -> int:
    problems: List[str] = []

    check_docs_exist(problems)
    check_skill_files(problems)

    if not problems:
        check_references(problems)
        check_ai_context_budgets(problems)
        check_active_decision_budget(problems)

    if problems:
        print("INSTRUCTION COHERENCE FAILED:")
        for problem in problems:
            print(f" - {problem}")
        return 1

    print("Instruction coherence OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
