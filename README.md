# dist-msg-platform

A Linux C++17 distributed message routing and file distribution platform that runs as multiple services on a single WSL Ubuntu 24.04 host. The backend uses `epoll + Reactor`, Protobuf for internal RPC, JSON for the debug/client endpoint, Redis for session and routing state, and MySQL for durable metadata. The project now also includes a browser dashboard built with `React + Vite` and a `FastAPI` bridge that exposes the raw TCP gateway to the web UI.

## Services

- `metadata_service`: owns MySQL schema and durable metadata for users, messages, files, and chunk records
- `session_service`: logs users in and keeps Redis session mappings fresh
- `router_service`: tracks gateway load, resolves online routes, and stores offline messages
- `file_service`: accepts chunked uploads, resume queries, and chunk downloads
- `gateway_service`: long-lived TCP JSON endpoint for clients and demo tooling
- `bridge/app.py`: FastAPI browser bridge for REST and WebSocket events
- `web/`: visual demo console and ops dashboard

## First Start

```bash
cd /home/lio/projects/dist-msg-platform
./scripts/build.sh
sudo ./scripts/init_db.sh
```

If this is the first time you want to use the visual console, make sure WSL also has `nodejs`, `npm`, and `python3-venv` installed.

## One-Click Backend Demo

```bash
cd /home/lio/projects/dist-msg-platform
./scripts/one_click_demo.sh
```

This command checks local dependencies, starts Redis and MySQL, initializes the MySQL demo database if needed, builds the project, starts all five services, waits for ports `7001/7101/7201/7301/7401`, and runs the automated message plus file-transfer demo.

## Web Visual Demo

```bash
cd /home/lio/projects/dist-msg-platform
./scripts/start_web_demo.sh
```

After startup:

- dashboard: `http://127.0.0.1:5173`
- bridge summary API: `http://127.0.0.1:8080/api/summary`
- event stream: `ws://127.0.0.1:8080/ws/events`

The dashboard includes:

- service status cards for the five backend services
- Alice/Bob interactive message panels
- offline message compensation actions
- file upload, pause, resume, and download verification
- an event timeline for demos and presentations

## Manual Startup

```bash
cd /home/lio/projects/dist-msg-platform
./scripts/build.sh
sudo ./scripts/init_db.sh
./scripts/start_stack.sh
python3 tools/demo_driver/demo_driver.py --host 127.0.0.1 --port 7001
```

## Manual Web Startup

```bash
cd /home/lio/projects/dist-msg-platform
python3 -m venv bridge/.venv
bridge/.venv/bin/pip install -r bridge/requirements.txt
cd web
npm install
VITE_API_BASE=http://127.0.0.1:8080 npm run dev -- --host 127.0.0.1 --port 5173
```

In a second terminal:

```bash
cd /home/lio/projects/dist-msg-platform
bridge/.venv/bin/python -m uvicorn bridge.app:app --app-dir /home/lio/projects/dist-msg-platform --host 127.0.0.1 --port 8080
```

## Manual Debug Commands

Summary:

```bash
python3 tools/json_debug_client/json_debug_client.py --host 127.0.0.1 --port 7001 '{"cmd":"summary"}'
```

Login:

```bash
python3 tools/json_debug_client/json_debug_client.py --host 127.0.0.1 --port 7001 '{"cmd":"login","user_id":"alice"}'
```

Send message:

```bash
python3 tools/json_debug_client/json_debug_client.py --host 127.0.0.1 --port 7001 '{"cmd":"send_message","from_user":"alice","to_user":"bob","body":"hello"}'
```

Pull offline:

```bash
python3 tools/json_debug_client/json_debug_client.py --host 127.0.0.1 --port 7001 '{"cmd":"pull_offline","user_id":"bob"}'
```

## Tests

Backend:

```bash
cd /home/lio/projects/dist-msg-platform/build
ctest --output-on-failure
```

Bridge:

```bash
cd /home/lio/projects/dist-msg-platform
python3 -m unittest bridge.tests.test_bridge
```

Frontend:

```bash
cd /home/lio/projects/dist-msg-platform/web
npm test
```

## Stop Services

Backend only:

```bash
cd /home/lio/projects/dist-msg-platform
./scripts/stop_stack.sh
```

Backend plus bridge and dashboard:

```bash
cd /home/lio/projects/dist-msg-platform
./scripts/stop_web_demo.sh
```

## Logs

- backend services: `logs/*.log`
- browser bridge: `logs/web_bridge.log`
- frontend dev server: `logs/web_frontend.log`
- pid files: `run/*.pid`

## Troubleshooting

- if startup fails, check whether ports `5173`, `7001`, `7101`, `7201`, `7301`, `7401`, and `8080` are already in use
- if MySQL or Redis is not running, rerun the startup scripts or manually start them with `sudo service`
- if the visual console is blank, first check `logs/web_bridge.log` and `logs/web_frontend.log`
- if `sudo` prompts appear, they are expected for database initialization and service startup