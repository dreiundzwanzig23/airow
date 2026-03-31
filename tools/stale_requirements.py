#!/usr/bin/env python3
"""Report stale OPEN requirements."""

from __future__ import annotations

import datetime as dt
import os
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
REQ = ROOT / "docs" / "process" / "REQUIREMENTS.md"
ID_RE = re.compile(r"^##\s+(R-\d{3})\b", re.M)
FIELD_RE = re.compile(r"^- \*\*(?P<k>[^*]+)\*\*:\s*(?P<v>.+)$", re.M)


def parse() -> dict[str, dict[str, str]]:
    text = REQ.read_text(encoding="utf-8")
    out: dict[str, dict[str, str]] = {}
    for m in ID_RE.finditer(text):
        rid = m.group(1)
        start = m.end()
        end = text.find("\n## ", start)
        block = text[start : end if end != -1 else None]
        fields = {fm.group("k").strip(): fm.group("v").strip() for fm in FIELD_RE.finditer(block)}
        out[rid] = fields
    return out


def main() -> int:
    stale_days = int(os.environ.get("STALE_DAYS", "45"))
    today = dt.date.today()
    stale = []

    for rid, fields in parse().items():
        if fields.get("Status") != "OPEN":
            continue
        created = fields.get("Created")
        if not created:
            continue
        try:
            created_date = dt.date.fromisoformat(created)
        except ValueError:
            print(f"Invalid Created date for {rid}: {created}")
            return 1
        age = (today - created_date).days
        if age > stale_days:
            stale.append((rid, age))

    if stale:
        print(f"Stale OPEN requirements (>{stale_days} days):")
        for rid, age in stale:
            print(f" - {rid}: {age} days")
        return 1

    print(f"No stale OPEN requirements older than {stale_days} days")
    return 0


if __name__ == "__main__":
    sys.exit(main())
