#!/usr/bin/env python3
"""Lint repo-local agent skill metadata and index coverage."""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path


_NAME_RE = re.compile(r"^[a-z0-9][a-z0-9-]*$")
_FRONT_MATTER_RE = re.compile(r"\A---\n(?P<meta>.*?)\n---\n", re.DOTALL)
_REQUIRED_FRONT_MATTER_KEYS = ("name", "description")


def _parse_front_matter(text: str) -> tuple[dict[str, str], str] | tuple[None, None]:
    match = _FRONT_MATTER_RE.match(text)
    if not match:
        return None, None

    metadata: dict[str, str] = {}
    for raw_line in match.group("meta").splitlines():
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue
        if ":" not in line:
            metadata[line] = ""
            continue
        key, value = line.split(":", 1)
        metadata[key.strip()] = value.strip()

    return metadata, text[match.end() :]


def _skill_dirs(root: Path) -> list[Path]:
    skills_root = root / ".agents" / "skills"
    if not skills_root.exists():
        return []
    return sorted(path for path in skills_root.iterdir() if path.is_dir())


def lint_root(root: Path) -> list[str]:
    """Return lint issues for the repository rooted at ``root``."""

    issues: list[str] = []
    root = root.resolve()
    skills_root = root / ".agents" / "skills"

    if not skills_root.exists():
        return ["missing .agents/skills directory"]

    names: dict[str, Path] = {}
    skill_paths: list[Path] = []

    for skill_dir in _skill_dirs(root):
        skill_path = skill_dir / "SKILL.md"
        rel_skill = skill_path.relative_to(root)
        if not skill_path.exists():
            issues.append(f"{skill_dir.relative_to(root)}: missing SKILL.md")
            continue

        skill_paths.append(skill_path)
        text = skill_path.read_text(encoding="utf-8")
        metadata, body = _parse_front_matter(text)
        if metadata is None or body is None:
            issues.append(f"{rel_skill}: missing valid YAML-style front matter")
            continue

        for key in _REQUIRED_FRONT_MATTER_KEYS:
            if not metadata.get(key):
                issues.append(f"{rel_skill}: missing front-matter key '{key}'")

        name = metadata.get("name", "")
        description = metadata.get("description", "")

        if name and not _NAME_RE.fullmatch(name):
            issues.append(f"{rel_skill}: invalid skill name '{name}'")
        if name and name != skill_dir.name:
            issues.append(
                f"{rel_skill}: skill name '{name}' does not match directory '{skill_dir.name}'"
            )
        if name:
            if name in names:
                issues.append(
                    f"{rel_skill}: duplicate skill name '{name}' also used by "
                    f"{names[name].relative_to(root)}"
                )
            names[name] = skill_path

        if description:
            if len(description) < 40:
                issues.append(f"{rel_skill}: description is too short for discovery")
            if "Use" not in description and "use" not in description:
                issues.append(
                    f"{rel_skill}: description should say when to use the skill"
                )

        if not body.lstrip().startswith("# "):
            issues.append(f"{rel_skill}: body should start with a top-level heading")
        if body.count("\n## ") < 2:
            issues.append(f"{rel_skill}: body should contain multiple operational sections")
        if "## Finish" not in body:
            issues.append(f"{rel_skill}: missing '## Finish' section")

    if not skill_paths:
        issues.append("no skill files found under .agents/skills")

    readme_path = skills_root / "README.md"
    if readme_path.exists():
        readme = readme_path.read_text(encoding="utf-8")
        for skill_path in skill_paths:
            rel = skill_path.relative_to(root).as_posix()
            if rel not in readme:
                issues.append(f".agents/skills/README.md: missing index entry for {rel}")
    else:
        issues.append(".agents/skills/README.md: missing skill index")

    agents_path = root / "AGENTS.md"
    if agents_path.exists():
        agents = agents_path.read_text(encoding="utf-8")
        for skill_path in skill_paths:
            rel = skill_path.relative_to(root).as_posix()
            if rel not in agents:
                issues.append(f"AGENTS.md: missing load-on-demand entry for {rel}")
    else:
        issues.append("AGENTS.md: missing policy index")

    return issues


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--root",
        type=Path,
        default=Path.cwd(),
        help="repository root to lint (default: current directory)",
    )
    args = parser.parse_args(argv)

    issues = lint_root(args.root)
    if issues:
        for issue in issues:
            print(f"skills_lint: {issue}", file=sys.stderr)
        return 1

    print("Skills lint OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
