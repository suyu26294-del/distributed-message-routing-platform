#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
RUN_DIR="${ROOT_DIR}/run"
services=(metadata_service session_service router_service file_service gateway_service)

for service in "${services[@]}"; do
  pid_file="${RUN_DIR}/${service}.pid"
  if [[ -f "${pid_file}" ]]; then
    pid="$(cat "${pid_file}")"
    kill "${pid}" >/dev/null 2>&1 || true
    rm -f "${pid_file}"
  fi
  pkill -x "${service}" >/dev/null 2>&1 || true
  for _ in {1..20}; do
    if ! pgrep -x "${service}" >/dev/null 2>&1; then
      break
    fi
    sleep 0.1
  done
done

echo "stack stopped"