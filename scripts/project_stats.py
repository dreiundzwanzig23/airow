#!/usr/bin/env python3
"""Condensed project statistics report for the template repository."""

from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, Iterable, List

ROOT = Path(__file__).resolve().parents[1]

CORE_PREFIXES = (
    "include/",
    "src/",
    "tests/",
    "scripts/",
    "tools/",
    "docs/ai/",
    "docs/process/",
    "skills/",
    "README.md",
    "CHANGELOG.md",
    "AGENTS.md",
    ".clang-tidy",
    "CMakeLists.txt",
    "CMakePresets.json",
)

CPP_EXTS = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx"}
TEXT_EXTS = {
    ".py",
    ".sh",
    ".md",
    ".txt",
    ".yaml",
    ".yml",
    ".json",
    ".cmake",
}


@dataclass
class LineStats:
    code: int = 0
    comments: int = 0
    blank: int = 0

    @property
    def total(self) -> int:
        return self.code + self.comments + self.blank

    def add(self, other: "LineStats") -> None:
        self.code += other.code
        self.comments += other.comments
        self.blank += other.blank


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Generate condensed project statistics for the template repo"
    )
    parser.add_argument(
        "--format",
        choices=["md", "json", "both", "agent"],
        default="md",
        help="Output format",
    )
    parser.add_argument(
        "--json",
        action="store_true",
        help="Alias for --format both when --format is omitted",
    )
    parser.add_argument(
        "--scope",
        choices=["core", "all"],
        default="core",
        help="File scope",
    )
    parser.add_argument(
        "--max-depth",
        type=int,
        default=2,
        help="Max folder depth for structure summary",
    )
    parser.add_argument(
        "--no-color",
        action="store_true",
        help="No-op for CI-safe plain output",
    )
    return parser.parse_args()


def run_cmd(args: List[str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(args, cwd=ROOT, check=False, text=True, capture_output=True)


def list_tracked_files() -> List[str]:
    proc = run_cmd(["git", "ls-files"])
    if proc.returncode != 0:
        return []
    return [line.strip() for line in proc.stdout.splitlines() if line.strip()]


def in_scope(path: str, scope: str) -> bool:
    if scope == "all":
        return True
    return any(path == prefix or path.startswith(prefix) for prefix in CORE_PREFIXES)


def region_for(path: str) -> str:
    if path.startswith("src/app/"):
        return "src/app"
    if path.startswith("src/lib/"):
        return "src/lib"
    if path.startswith("include/project/"):
        return "include/project"
    if path.startswith("tests/unit/"):
        return "tests/unit"
    if path.startswith("tests/integration/"):
        return "tests/integration"
    if path.startswith("tests/system/"):
        return "tests/system"
    if path.startswith("scripts/"):
        return "scripts"
    if path.startswith("tools/"):
        return "tools"
    if path.startswith("docs/ai/"):
        return "docs/ai"
    if path.startswith("docs/process/"):
        return "docs/process"
    if path.startswith("skills/"):
        return "skills"
    if path.startswith("tests/"):
        return "tests/other"
    if path.startswith("src/"):
        return "src/other"
    if path.startswith("include/"):
        return "include/other"
    if path.startswith("docs/"):
        return "docs/other"
    return "other"


def classify_lines(path: Path) -> LineStats:
    stats = LineStats()
    try:
        text = path.read_text(encoding="utf-8")
    except UnicodeDecodeError:
        text = path.read_text(encoding="latin-1")
    except OSError:
        return stats

    ext = path.suffix.lower()
    in_block_comment = False

    for raw in text.splitlines():
        line = raw.strip()
        if not line:
            stats.blank += 1
            continue

        if ext in CPP_EXTS:
            if in_block_comment:
                stats.comments += 1
                if "*/" in line:
                    in_block_comment = False
                continue
            if line.startswith("//"):
                stats.comments += 1
                continue
            if line.startswith("/*"):
                stats.comments += 1
                if "*/" not in line:
                    in_block_comment = True
                continue
            stats.code += 1
            continue

        if ext == ".py":
            if line.startswith("#"):
                stats.comments += 1
            else:
                stats.code += 1
            continue

        if ext in {".sh", ".bash"}:
            if line.startswith("#"):
                stats.comments += 1
            else:
                stats.code += 1
            continue

        if ext in TEXT_EXTS:
            stats.code += 1
            continue

        stats.code += 1

    return stats


def regex_function_count(cpp_paths: Iterable[Path]) -> Dict[str, object]:
    pattern = re.compile(
        r"^\s*(?:template\s*<[^>]+>\s*)?(?:[\w:<>\*&\s]+)\s+"
        r"(?:[\w:~]+)\s*\([^;]*\)\s*(?:const\s*)?(?:noexcept\s*)?(?:\{|$)"
    )

    count = 0
    for path in cpp_paths:
        try:
            lines = path.read_text(encoding="utf-8", errors="ignore").splitlines()
        except OSError:
            continue
        for idx, line in enumerate(lines):
            if not pattern.match(line):
                continue
            if line.rstrip().endswith(";"):
                continue
            if line.rstrip().endswith("{"):
                count += 1
                continue
            lookahead = lines[idx + 1].strip() if idx + 1 < len(lines) else ""
            if lookahead == "{" or lookahead.startswith("{"):
                count += 1

    return {
        "function_count": count,
        "warning_count": None,
        "avg_ccn": None,
        "function_count_mode": "regex_fallback",
        "regex_fallback": True,
    }


def lizard_function_count() -> Dict[str, object]:
    proc = run_cmd(["lizard", "-l", "cpp", "include", "src", "tests"])
    if proc.returncode not in (0, 1):
        return {}

    summary_match = re.search(
        r"^\s*(\d+)\s+(\d+\.\d+)\s+(\d+\.\d+)\s+(\d+\.\d+)\s+(\d+)\s+(\d+)\s+(\d+\.\d+)\s+(\d+\.\d+)\s*$",
        proc.stdout,
        flags=re.MULTILINE,
    )
    if not summary_match:
        return {}

    return {
        "total_nloc": int(summary_match.group(1)),
        "avg_nloc": float(summary_match.group(2)),
        "avg_ccn": float(summary_match.group(3)),
        "avg_token": float(summary_match.group(4)),
        "function_count": int(summary_match.group(5)),
        "warning_count": int(summary_match.group(6)),
        "function_count_mode": "lizard",
    }


def structure_summary(paths: List[str], max_depth: int) -> Dict[str, int]:
    counts: Dict[str, int] = {}
    for path in paths:
        parts = path.split("/")
        depth = min(len(parts), max_depth)
        key = "/".join(parts[:depth]) if depth > 0 else path
        counts[key] = counts.get(key, 0) + 1
    return dict(sorted(counts.items()))


def gather_stats(scope: str, max_depth: int) -> Dict[str, object]:
    tracked = [path for path in list_tracked_files() if in_scope(path, scope)]
    region_stats: Dict[str, LineStats] = {}
    cpp_paths: List[Path] = []

    for rel in tracked:
        path = ROOT / rel
        if not path.is_file():
            continue
        stats = classify_lines(path)
        region = region_for(rel)
        region_stats.setdefault(region, LineStats()).add(stats)
        if path.suffix.lower() in CPP_EXTS:
            cpp_paths.append(path)

    totals = LineStats()
    for stats in region_stats.values():
        totals.add(stats)

    lizard_stats = lizard_function_count()
    if not lizard_stats:
        lizard_stats = regex_function_count(cpp_paths)

    return {
        "scope": scope,
        "tracked_file_count": len(tracked),
        "regions": {
            region: {
                "code": stats.code,
                "comments": stats.comments,
                "blank": stats.blank,
                "total": stats.total,
            }
            for region, stats in sorted(region_stats.items())
        },
        "totals": {
            "code": totals.code,
            "comments": totals.comments,
            "blank": totals.blank,
            "total": totals.total,
        },
        "structure": structure_summary(tracked, max_depth),
        "functions": lizard_stats,
    }


def render_md(stats: Dict[str, object]) -> str:
    lines = [
        "# Project Stats",
        "",
        f"- Scope: `{stats['scope']}`",
        f"- Tracked files: `{stats['tracked_file_count']}`",
        f"- Total lines: `{stats['totals']['total']}`",
        f"- Code lines: `{stats['totals']['code']}`",
        f"- Comment lines: `{stats['totals']['comments']}`",
        f"- Blank lines: `{stats['totals']['blank']}`",
        "",
        "## Regions",
        "",
        "| Region | Code | Comments | Blank | Total |",
        "|--------|------|----------|-------|-------|",
    ]

    for region, region_stats in stats["regions"].items():
        lines.append(
            f"| {region} | {region_stats['code']} | {region_stats['comments']} | "
            f"{region_stats['blank']} | {region_stats['total']} |"
        )

    lines.extend(
        [
            "",
            "## Functions",
            "",
            f"- Count mode: `{stats['functions'].get('function_count_mode', 'unknown')}`",
            f"- Function count: `{stats['functions'].get('function_count', 'n/a')}`",
            "",
            "## Structure",
            "",
        ]
    )

    for key, count in stats["structure"].items():
        lines.append(f"- `{key}`: {count}")

    lines.append("")
    return "\n".join(lines)


def render_agent(stats: Dict[str, object]) -> str:
    top_regions = sorted(
        stats["regions"].items(),
        key=lambda item: item[1]["total"],
        reverse=True,
    )[:8]
    structure_items = list(stats["structure"].items())[:20]

    lines = [
        "Project orientation:",
        f"- tracked files: {stats['tracked_file_count']}",
        f"- total lines: {stats['totals']['total']}",
        f"- function count: {stats['functions'].get('function_count', 'n/a')} "
        f"({stats['functions'].get('function_count_mode', 'unknown')})",
        "- largest regions:",
    ]

    for region, region_stats in top_regions:
        lines.append(f"  - {region}: {region_stats['total']} lines")

    lines.append("- structure summary:")
    for key, count in structure_items:
        lines.append(f"  - {key}: {count} file(s)")

    return "\n".join(lines)


def main() -> int:
    args = parse_args()
    if args.json and args.format == "md":
        args.format = "both"

    stats = gather_stats(args.scope, args.max_depth)

    if args.format == "json":
        print(json.dumps(stats, indent=2))
        return 0
    if args.format == "agent":
        print(render_agent(stats))
        return 0
    if args.format == "both":
        print(render_md(stats))
        print(json.dumps(stats, indent=2))
        return 0

    print(render_md(stats))
    return 0


if __name__ == "__main__":
    sys.exit(main())
