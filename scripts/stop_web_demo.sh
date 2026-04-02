#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
RUN_DIR="${ROOT_DIR}/run"

stop_by_pid() {
  local name="$1"
  local pid_file="${RUN_DIR}/${name}.pid"
  if [[ -f "${pid_file}" ]]; then
    kill "$(cat "${pid_file}")" >/dev/null 2>&1 || true
    rm -f "${pid_file}"
  fi
}

stop_by_pid web_bridge
stop_by_pid web_frontend

pkill -f "uvicorn bridge.app:app" >/dev/null 2>&1 || true
pkill -f "http.server 5173" >/dev/null 2>&1 || true
pkill -f "vite.*5173" >/dev/null 2>&1 || true

"${ROOT_DIR}/scripts/stop_stack.sh"

echo "web demo stopped"