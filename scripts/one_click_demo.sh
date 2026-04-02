#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

require_cmd() {
  local name="$1"
  if ! command -v "${name}" >/dev/null 2>&1; then
    echo "missing required command: ${name}" >&2
    exit 1
  fi
}

run_privileged() {
  if [[ "${EUID}" -eq 0 ]]; then
    "$@"
  else
    sudo "$@"
  fi
}

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

check_requirements() {
  require_cmd cmake
  require_cmd protoc
  require_cmd python3
  require_cmd redis-server
  require_cmd mysql
  require_cmd ss
}

ensure_database() {
  if ! mysql -h127.0.0.1 -udist_platform -pdist_platform dist_platform -e "SELECT 1" >/dev/null 2>&1; then
    echo "[demo] initializing MySQL database and user"
    run_privileged "${ROOT_DIR}/scripts/init_db.sh"
  fi
}

echo "[demo] checking local requirements"
check_requirements

echo "[demo] starting Redis and MySQL"
run_privileged service redis-server start >/dev/null
run_privileged service mysql start >/dev/null

echo "[demo] ensuring database access"
ensure_database

echo "[demo] building project"
"${ROOT_DIR}/scripts/build.sh"

echo "[demo] starting service stack"
"${ROOT_DIR}/scripts/start_stack.sh"

for port in 7401 7101 7201 7301 7001; do
  if ! wait_for_port "${port}" 50; then
    echo "port ${port} did not become ready" >&2
    exit 1
  fi
done

echo "[demo] running end-to-end demo"
python3 "${ROOT_DIR}/tools/demo_driver/demo_driver.py" --host 127.0.0.1 --port 7001

echo
echo "[demo] stack is still running"
echo "[demo] manual summary:"
echo "python3 ${ROOT_DIR}/tools/json_debug_client/json_debug_client.py --host 127.0.0.1 --port 7001 '{\"cmd\":\"summary\"}'"
echo "[demo] stop all services:"
echo "${ROOT_DIR}/scripts/stop_stack.sh"