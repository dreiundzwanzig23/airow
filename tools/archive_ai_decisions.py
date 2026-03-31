#!/usr/bin/env python3
"""Archive older ADR entries from docs/ai/DECISIONS.md."""

from __future__ import annotations

import argparse
import datetime as dt
import pathlib
import re
import sys
from dataclasses import dataclass
from typing import Dict, List, Tuple

ROOT = pathlib.Path(__file__).resolve().parents[1]
DECISIONS_PATH = ROOT / "docs" / "ai" / "DECISIONS.md"
ARCHIVE_DIR = ROOT / "docs" / "ai" / "archive"
ARCHIVE_INDEX_PATH = ARCHIVE_DIR / "DECISIONS_INDEX.md"
DEFAULT_MAX_ACTIVE_ADRS = 8

ADR_HEADER = re.compile(
    r"^##\s+(?P<id>ADR-(?P<date>\d{4}-\d{2}-\d{2})-\d{3})\s*$", re.MULTILINE
)
ARCHIVE_FILE_RE = re.compile(r"^DECISIONS_pre_\d{4}-\d{2}-\d{2}\.md$")
INDEX_POINTER = "Archived ADR index: `docs/ai/archive/DECISIONS_INDEX.md`."


@dataclass(frozen=True)
class AdrBlock:
    adr_id: str
    date: dt.date
    body: str


def parse_adr_blocks(text: str) -> Tuple[str, List[AdrBlock]]:
    matches = list(ADR_HEADER.finditer(text))
    if not matches:
        return text, []

    preamble = text[: matches[0].start()]
    blocks: List[AdrBlock] = []
    for idx, match in enumerate(matches):
        start = match.start()
        end = matches[idx + 1].start() if idx + 1 < len(matches) else len(text)
        adr_id = match.group("id")
        adr_date = dt.date.fromisoformat(match.group("date"))
        body = text[start:end].strip() + "\n"
        blocks.append(AdrBlock(adr_id=adr_id, date=adr_date, body=body))
    return preamble, blocks


def ensure_archive_index_pointer(preamble: str) -> str:
    if INDEX_POINTER in preamble:
        return preamble
    trimmed = preamble.rstrip()
    if trimmed:
        return f"{trimmed}\n\n{INDEX_POINTER}\n"
    return f"{INDEX_POINTER}\n"


def compose_active_decisions(preamble: str, kept_blocks: List[AdrBlock]) -> str:
    out = ensure_archive_index_pointer(preamble).rstrip() + "\n\n"
    if kept_blocks:
        out += "\n".join(block.body.rstrip() for block in kept_blocks) + "\n"
    return out


def archive_path_for_today(today: dt.date) -> pathlib.Path:
    return ARCHIVE_DIR / f"DECISIONS_pre_{today.isoformat()}.md"


def parse_archive_ids(path: pathlib.Path) -> Dict[str, AdrBlock]:
    if not path.exists():
        return {}
    _, blocks = parse_adr_blocks(path.read_text(encoding="utf-8"))
    return {block.adr_id: block for block in blocks}


def write_archive_file(
    path: pathlib.Path, existing: Dict[str, AdrBlock], additions: List[AdrBlock]
) -> None:
    merged: Dict[str, AdrBlock] = dict(existing)
    for block in additions:
        merged.setdefault(block.adr_id, block)

    ordered = sorted(merged.values(), key=lambda b: (b.date, b.adr_id), reverse=True)
    header = [
        f"# DECISIONS Archive — pre {path.stem.replace('DECISIONS_pre_', '')}",
        "",
        "Archived ADR blocks moved from `docs/ai/DECISIONS.md`.",
        "",
    ]
    body = "\n".join(block.body.rstrip() for block in ordered)
    text = "\n".join(header) + (body + "\n" if body else "")
    path.write_text(text, encoding="utf-8")


def archived_adr_rows() -> List[Tuple[str, dt.date, str]]:
    rows: List[Tuple[str, dt.date, str]] = []
    for path in sorted(ARCHIVE_DIR.iterdir()):
        if not path.is_file() or not ARCHIVE_FILE_RE.match(path.name):
            continue
        _, blocks = parse_adr_blocks(path.read_text(encoding="utf-8"))
        for block in blocks:
            rows.append((block.adr_id, block.date, path.name))
    dedup: Dict[str, Tuple[str, dt.date, str]] = {}
    for row in rows:
        dedup.setdefault(row[0], row)
    return sorted(dedup.values(), key=lambda r: (r[1], r[0]), reverse=True)


def write_archive_index() -> None:
    rows = archived_adr_rows()
    lines = [
        "# DECISIONS_INDEX.md",
        "",
        "Index of archived ADR entries moved out of active `docs/ai/DECISIONS.md`.",
        "",
        "| ADR | Date | Archive File |",
        "|-----|------|--------------|",
    ]
    if rows:
        for adr_id, adr_date, file_name in rows:
            lines.append(f"| {adr_id} | {adr_date.isoformat()} | `{file_name}` |")
    else:
        lines.append("| — | — | — |")
    lines.append("")
    ARCHIVE_INDEX_PATH.write_text("\n".join(lines), encoding="utf-8")


def rotate_decisions(max_active_adrs: int) -> int:
    if not DECISIONS_PATH.exists():
        print(f"Missing required file: {DECISIONS_PATH}")
        return 1
    ARCHIVE_DIR.mkdir(parents=True, exist_ok=True)

    original = DECISIONS_PATH.read_text(encoding="utf-8")
    preamble, blocks = parse_adr_blocks(original)
    if len(blocks) <= max_active_adrs:
        DECISIONS_PATH.write_text(
            compose_active_decisions(preamble, blocks), encoding="utf-8"
        )
        write_archive_index()
        print(
            f"No archival needed: {len(blocks)} ADRs <= max_active_adrs={max_active_adrs}"
        )
        return 0

    ordered = sorted(blocks, key=lambda b: (b.date, b.adr_id), reverse=True)
    kept = ordered[:max_active_adrs]
    archived = ordered[max_active_adrs:]

    DECISIONS_PATH.write_text(compose_active_decisions(preamble, kept), encoding="utf-8")

    archive_path = archive_path_for_today(dt.date.today())
    existing = parse_archive_ids(archive_path)
    write_archive_file(archive_path, existing, archived)
    write_archive_index()
    print(
        f"Archived {len(archived)} ADRs to {archive_path.relative_to(ROOT)}; "
        f"kept {len(kept)} active ADRs."
    )
    return 0


def main(argv: List[str]) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--max-active-adrs",
        type=int,
        default=DEFAULT_MAX_ACTIVE_ADRS,
        help="Maximum number of newest ADR blocks kept in docs/ai/DECISIONS.md",
    )
    args = parser.parse_args(argv)
    if args.max_active_adrs <= 0:
        print("--max-active-adrs must be > 0")
        return 2
    return rotate_decisions(args.max_active_adrs)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
