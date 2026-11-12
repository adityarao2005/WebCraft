#!/usr/bin/env bash
# Keep the existing entry point; Python owns process cleanup and result parsing.
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec python3 "${SCRIPT_DIR}/run_benchmarks.py" "$@"
