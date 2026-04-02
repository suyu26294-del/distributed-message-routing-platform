#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BRIDGE_DIR="${ROOT_DIR}/bridge"
WEB_DIR="${ROOT_DIR}/web"
RUN_DIR="${ROOT_DIR}/run"
LOG_DIR="${ROOT_DIR}/logs"
BRIDGE_VENV="${BRIDGE_DIR}/.venv"

mkdir -p "${RUN_DIR}" "${LOG_DIR}"

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
  local retries="${2:-80}"
  local i
  for ((i = 0; i < retries; ++i)); do
    if ss -ltn | grep -q ":${port} "; then
      return 0
    fi
    sleep 0.25
  done
  return 1
}

ensure_database() {
  if ! mysql -h127.0.0.1 -udist_platform -pdist_platform dist_platform -e "SELECT 1" >/dev/null 2>&1; then
    echo "[web-demo] initializing MySQL database and user"
    run_privileged "${ROOT_DIR}/scripts/init_db.sh"
  fi
}

ensure_bridge_deps() {
  if [[ ! -x "${BRIDGE_VENV}/bin/python" ]]; then
    python3 -m venv "${BRIDGE_VENV}"
  fi
  if ! "${BRIDGE_VENV}/bin/python" -c "import fastapi, uvicorn, pydantic, httpx" >/dev/null 2>&1; then
    "${BRIDGE_VENV}/bin/pip" install --disable-pip-version-check -r "${BRIDGE_DIR}/requirements.txt"
  fi
}

ensure_web_deps() {
  if [[ ! -d "${WEB_DIR}/node_modules" ]]; then
    (cd "${WEB_DIR}" && npm install)
  fi
}

check_requirements() {
  require_cmd cmake
  require_cmd protoc
  require_cmd python3
  require_cmd node
  require_cmd npm
  require_cmd mysql
  require_cmd redis-server
  require_cmd ss
}

echo "[web-demo] checking local requirements"
check_requirements

echo "[web-demo] ensuring Redis and MySQL"
run_privileged service redis-server start >/dev/null
run_privileged service mysql start >/dev/null

ensure_database

echo "[web-demo] building C++ services"
"${ROOT_DIR}/scripts/build.sh"

echo "[web-demo] starting backend stack"
"${ROOT_DIR}/scripts/start_stack.sh"

echo "[web-demo] preparing bridge runtime"
ensure_bridge_deps

echo "[web-demo] preparing React dashboard"
ensure_web_deps
(cd "${WEB_DIR}" && VITE_API_BASE='http://127.0.0.1:8080' npm run build >"${LOG_DIR}/web_build.log" 2>&1)

pkill -f "uvicorn bridge.app:app" >/dev/null 2>&1 || true
rm -f "${RUN_DIR}/web_bridge.pid"
nohup "${BRIDGE_VENV}/bin/python" -m uvicorn bridge.app:app --app-dir "${ROOT_DIR}" --host 127.0.0.1 --port 8080 >"${LOG_DIR}/web_bridge.log" 2>&1 < /dev/null &
echo $! > "${RUN_DIR}/web_bridge.pid"

if ! wait_for_port 8080 80; then
  echo "bridge failed to start on port 8080" >&2
  tail -n 40 "${LOG_DIR}/web_bridge.log" >&2 || true
  exit 1
fi

pkill -f "http.server 5173" >/dev/null 2>&1 || true
rm -f "${RUN_DIR}/web_frontend.pid"
nohup python3 -m http.server 5173 --bind 127.0.0.1 --directory "${WEB_DIR}/dist" >"${LOG_DIR}/web_frontend.log" 2>&1 < /dev/null &
echo $! > "${RUN_DIR}/web_frontend.pid"

if ! wait_for_port 5173 80; then
  echo "frontend failed to start on port 5173" >&2
  tail -n 40 "${LOG_DIR}/web_frontend.log" >&2 || true
  exit 1
fi

echo
echo "[web-demo] visual console is ready"
echo "[web-demo] dashboard: http://127.0.0.1:5173"
echo "[web-demo] bridge api: http://127.0.0.1:8080/api/summary"
echo "[web-demo] logs: ${LOG_DIR}/web_bridge.log, ${LOG_DIR}/web_frontend.log, ${LOG_DIR}/web_build.log"
echo "[web-demo] stop everything: ${ROOT_DIR}/scripts/stop_web_demo.sh"