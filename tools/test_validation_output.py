#!/usr/bin/env python3
"""Regression checks for scripts/validation_output.sh."""

from __future__ import annotations

import json
from pathlib import Path
import stat
import subprocess
import tempfile
import textwrap

ROOT = Path(__file__).resolve().parents[1]


def write_executable(path: Path, contents: str) -> None:
    path.write_text(contents, encoding="utf-8")
    path.chmod(path.stat().st_mode | stat.S_IXUSR)


def run_wrapper(child_contents: str, expected_returncode: int, expected_status: str) -> None:
    with tempfile.TemporaryDirectory(prefix="airow-validation-output-") as tmp:
      tmpdir = Path(tmp)
      logs_dir = tmpdir / "logs"
      logs_dir.mkdir()

      child_path = tmpdir / "child.sh"
      write_executable(
          child_path,
          textwrap.dedent(child_contents),
      )

      wrapper_path = tmpdir / "wrapper.sh"
      write_executable(
          wrapper_path,
          textwrap.dedent(
              f"""\
              #!/usr/bin/env bash
              set -euo pipefail
              source {str((ROOT / "scripts" / "validation_output.sh")).__repr__()}
              validation_run_logged "child-step" {str(child_path).__repr__()}
              """
          ),
      )

      summary_path = logs_dir / "wrapper-summary.json"
      completed = subprocess.run(
          ["bash", str(wrapper_path)],
          cwd=ROOT,
          env={
              "PATH": str(Path("/usr/bin")) + ":" + str(Path("/bin")),
              "VALIDATION_LOG_DIR": str(logs_dir),
              "VALIDATION_SUMMARY_DIR": str(logs_dir),
              "VALIDATION_SUMMARY_PATH": str(summary_path),
          },
          check=False,
          text=True,
          capture_output=True,
      )

      if completed.returncode != expected_returncode:
          raise AssertionError(
              f"wrapper returned {completed.returncode}, expected {expected_returncode}\n"
              f"stdout:\n{completed.stdout}\n"
              f"stderr:\n{completed.stderr}"
          )

      summary = json.loads(summary_path.read_text(encoding="utf-8"))
      if summary["status"] != expected_status:
          raise AssertionError(
              f"summary status was {summary['status']!r}, expected {expected_status!r}"
          )
      if len(summary["steps"]) != 1:
          raise AssertionError(f"expected one step, got {len(summary['steps'])}")

      step = summary["steps"][0]
      if step["name"] != "child-step":
          raise AssertionError(f"unexpected step name {step['name']!r}")
      if step["status"] != expected_status:
          raise AssertionError(
              f"step status was {step['status']!r}, expected {expected_status!r}"
          )
      if step["exit_code"] != expected_returncode:
          raise AssertionError(
              f"step exit_code was {step['exit_code']}, expected {expected_returncode}"
          )


def main() -> int:
    run_wrapper(
        """\
        #!/usr/bin/env bash
        exit 0
        """,
        expected_returncode=0,
        expected_status="pass",
    )
    run_wrapper(
        """\
        #!/usr/bin/env bash
        exit 7
        """,
        expected_returncode=7,
        expected_status="fail",
    )
    print("Validation output contracts OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
