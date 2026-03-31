#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=./validation_output.sh
source "${script_dir}/validation_output.sh"

validation_run_logged "depcheck-ai-decisions-archive" \
  python3 tools/archive_ai_decisions.py
validation_run_logged "depcheck-dependency-rules" python3 tools/depcheck.py
validation_run_logged "depcheck-instruction-coherence" \
  python3 tools/check_instruction_coherence.py
