#!/usr/bin/env python3
"""Regression tests for tools/skills_lint.py."""

from __future__ import annotations

import tempfile
import unittest
from pathlib import Path

import skills_lint


VALID_SKILL = """---
name: example-skill
description: Use when testing that a valid skill fixture passes repository skill linting.
---

# Example Skill

## Start
- Begin with a clear task.

## Execute
- Do the work.

## Finish
- End cleanly.
"""


class SkillsLintTests(unittest.TestCase):
    def make_repo(self) -> Path:
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        root = Path(tmp.name)
        skill_dir = root / ".agents" / "skills" / "example-skill"
        skill_dir.mkdir(parents=True)
        (skill_dir / "SKILL.md").write_text(VALID_SKILL, encoding="utf-8")
        (root / ".agents" / "skills" / "README.md").write_text(
            "# Repository Skills\n\n- `.agents/skills/example-skill/SKILL.md`\n",
            encoding="utf-8",
        )
        (root / "AGENTS.md").write_text(
            "## Repository Skills\n- `.agents/skills/example-skill/SKILL.md`\n",
            encoding="utf-8",
        )
        return root

    def test_valid_skill_tree_passes(self) -> None:
        root = self.make_repo()
        self.assertEqual([], skills_lint.lint_root(root))

    def test_invalid_front_matter_fails(self) -> None:
        root = self.make_repo()
        skill_path = root / ".agents" / "skills" / "example-skill" / "SKILL.md"
        skill_path.write_text("# Missing Front Matter\n", encoding="utf-8")

        issues = skills_lint.lint_root(root)

        self.assertTrue(any("front matter" in issue for issue in issues), issues)

    def test_name_must_match_directory(self) -> None:
        root = self.make_repo()
        skill_path = root / ".agents" / "skills" / "example-skill" / "SKILL.md"
        skill_path.write_text(
            VALID_SKILL.replace("name: example-skill", "name: other-skill"),
            encoding="utf-8",
        )

        issues = skills_lint.lint_root(root)

        self.assertTrue(any("does not match directory" in issue for issue in issues), issues)

    def test_indexes_must_reference_every_skill(self) -> None:
        root = self.make_repo()
        (root / "AGENTS.md").write_text("## Repository Skills\n", encoding="utf-8")

        issues = skills_lint.lint_root(root)

        self.assertTrue(any("AGENTS.md" in issue for issue in issues), issues)


if __name__ == "__main__":
    unittest.main()
