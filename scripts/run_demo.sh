#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

run_privileged() {
  if [[ "${EUID}" -eq 0 ]]; then
    "$@"
  else
    sudo "$@"
  fi
}

"${ROOT_DIR}/scripts/build.sh"
run_privileged "${ROOT_DIR}/scripts/init_db.sh"
"${ROOT_DIR}/scripts/start_stack.sh"
python3 "${ROOT_DIR}/tools/demo_driver/demo_driver.py" --host 127.0.0.1 --port 7001