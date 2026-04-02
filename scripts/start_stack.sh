#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build"
RUN_DIR="${ROOT_DIR}/run"
LOG_DIR="${ROOT_DIR}/logs"

mkdir -p "${RUN_DIR}" "${LOG_DIR}"

if [[ ! -x "${BUILD_DIR}/metadata_service" ]]; then
  "${ROOT_DIR}/scripts/build.sh"
fi

run_privileged() {
  if [[ "${EUID}" -eq 0 ]]; then
    "$@"
  else
    sudo "$@"
  fi
}

run_privileged service redis-server start >/dev/null
run_privileged service mysql start >/dev/null

wait_for_port() {
  local port="$1"
  local retries="${2:-50}"
  local i
  for ((i = 0; i < retries; ++i)); do
    if ss -ltn | grep -q ":${port} "; then
      return 0
    fi
    sleep 0.2
  done
  return 1
}

run_service() {
  local name="$1"
  local port="$2"
  shift 2

  pkill -f "${BUILD_DIR}/${name}" >/dev/null 2>&1 || true
  rm -f "${RUN_DIR}/${name}.pid"
  nohup "${BUILD_DIR}/${name}" "$@" >"${LOG_DIR}/${name}.log" 2>&1 < /dev/null &
  echo $! > "${RUN_DIR}/${name}.pid"

  if ! wait_for_port "${port}" 50; then
    echo "failed to start ${name} on port ${port}" >&2
    [[ -f "${LOG_DIR}/${name}.log" ]] && tail -n 20 "${LOG_DIR}/${name}.log" >&2 || true
    exit 1
  fi
}

run_service metadata_service 7401 "${ROOT_DIR}/config/metadata.json"
run_service session_service 7101 "${ROOT_DIR}/config/session.json"
run_service router_service 7201 "${ROOT_DIR}/config/router.json"
run_service file_service 7301 "${ROOT_DIR}/config/file.json"
run_service gateway_service 7001 "${ROOT_DIR}/config/gateway.json"

echo "stack started"