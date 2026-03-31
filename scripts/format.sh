#!/usr/bin/env bash
set -euo pipefail
command -v clang-format >/dev/null 2>&1 || { echo >&2 "clang-format not found"; exit 1; }
git ls-files '*.hpp' '*.h' '*.cpp' | while IFS= read -r path; do
  if [ -f "${path}" ]; then
    printf '%s\0' "${path}"
  fi
done | xargs -0 clang-format -i
