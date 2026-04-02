from __future__ import annotations

import asyncio
import base64
import json
import os
import queue
import socket
import struct
import threading
from datetime import datetime, timezone
from typing import Any

from fastapi import FastAPI, HTTPException, WebSocket, WebSocketDisconnect
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel, Field

GATEWAY_HOST = os.getenv("DIST_PLATFORM_GATEWAY_HOST", "127.0.0.1")
GATEWAY_PORT = int(os.getenv("DIST_PLATFORM_GATEWAY_PORT", "7001"))
SUMMARY_POLL_SECONDS = float(os.getenv("DIST_PLATFORM_SUMMARY_POLL_SECONDS", "2.0"))
DISABLE_POLLER = os.getenv("DIST_PLATFORM_DISABLE_POLLER", "0") == "1"

SERVICE_PORTS = [
    {"name": "gateway_service", "port": 7001, "label": "Gateway"},
    {"name": "session_service", "port": 7101, "label": "Session"},
    {"name": "router_service", "port": 7201, "label": "Router"},
    {"name": "file_service", "port": 7301, "label": "File"},
    {"name": "metadata_service", "port": 7401, "label": "Metadata"},
]


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat()


class EventHub:
    def __init__(self) -> None:
        self._subscribers: set[queue.Queue[dict[str, Any]]] = set()
        self._lock = threading.Lock()

    def subscribe(self) -> queue.Queue[dict[str, Any]]:
        subscriber: queue.Queue[dict[str, Any]] = queue.Queue()
        with self._lock:
            self._subscribers.add(subscriber)
        return subscriber

    def unsubscribe(self, subscriber: queue.Queue[dict[str, Any]]) -> None:
        with self._lock:
            self._subscribers.discard(subscriber)

    def broadcast(self, event: dict[str, Any]) -> None:
        payload = {"timestamp": utc_now(), **event}
        with self._lock:
            subscribers = list(self._subscribers)
        for subscriber in subscribers:
            subscriber.put(payload)


hub = EventHub()


def send_json_frame(sock: socket.socket, payload: dict[str, Any]) -> None:
    data = json.dumps(payload).encode("utf-8")
    sock.sendall(struct.pack("!I", len(data)) + data)


def recv_exact(sock: socket.socket, size: int) -> bytes:
    data = bytearray()
    while len(data) < size:
        chunk = sock.recv(size - len(data))
        if not chunk:
            raise RuntimeError("connection closed")
        data.extend(chunk)
    return bytes(data)


def recv_json_frame(sock: socket.socket) -> dict[str, Any]:
    header = recv_exact(sock, 4)
    length = struct.unpack("!I", header)[0]
    payload = recv_exact(sock, length)
    return json.loads(payload.decode("utf-8"))


def send_frame_once(payload: dict[str, Any]) -> dict[str, Any]:
    with socket.create_connection((GATEWAY_HOST, GATEWAY_PORT), timeout=5) as sock:
        send_json_frame(sock, payload)
        return recv_json_frame(sock)


class GatewaySession:
    def __init__(self, user_id: str) -> None:
        self.user_id = user_id
        self._sock = socket.create_connection((GATEWAY_HOST, GATEWAY_PORT), timeout=5)
        self._sock.settimeout(None)
        self._send_lock = threading.Lock()
        self._responses: queue.Queue[dict[str, Any]] = queue.Queue()
        self._alive = True
        self._reader = threading.Thread(target=self._reader_loop, daemon=True)
        self._reader.start()

    @property
    def is_alive(self) -> bool:
        return self._alive

    def _reader_loop(self) -> None:
        try:
            while self._alive:
                payload = recv_json_frame(self._sock)
                if payload.get("type") == "message":
                    hub.broadcast({"type": "live_message", "user_id": self.user_id, "message": payload})
                else:
                    self._responses.put(payload)
        except Exception as exc:
            if self._alive:
                hub.broadcast({"type": "session_closed", "user_id": self.user_id, "message": str(exc)})
        finally:
            self._alive = False

    def send(self, payload: dict[str, Any], timeout: float = 5.0) -> dict[str, Any]:
        if not self._alive:
            raise RuntimeError(f"session for {self.user_id} is not connected")
        with self._send_lock:
            send_json_frame(self._sock, payload)
            try:
                return self._responses.get(timeout=timeout)
            except queue.Empty as exc:
                raise RuntimeError(f"timed out waiting for gateway response to {payload.get('cmd')}") from exc

    def close(self) -> None:
        self._alive = False
        try:
            self._sock.shutdown(socket.SHUT_RDWR)
        except OSError:
            pass
        try:
            self._sock.close()
        except OSError:
            pass


class GatewaySessionManager:
    def __init__(self) -> None:
        self._sessions: dict[str, GatewaySession] = {}
        self._lock = threading.Lock()

    def login(self, user_id: str) -> dict[str, Any]:
        with self._lock:
            previous = self._sessions.pop(user_id, None)
        if previous is not None:
            previous.close()

        session = GatewaySession(user_id)
        response = session.send({"cmd": "login", "user_id": user_id})
        if not response.get("ok"):
            session.close()
            raise RuntimeError(response.get("message", "login failed"))

        with self._lock:
            self._sessions[user_id] = session
        return response

    def _get_live_session(self, user_id: str) -> GatewaySession | None:
        with self._lock:
            session = self._sessions.get(user_id)
        if session is None:
            return None
        if not session.is_alive:
            with self._lock:
                current = self._sessions.get(user_id)
                if current is session:
                    self._sessions.pop(user_id, None)
            return None
        return session

    def ensure_session(self, user_id: str) -> GatewaySession:
        session = self._get_live_session(user_id)
        if session is not None:
            return session
        self.login(user_id)
        session = self._get_live_session(user_id)
        if session is None:
            raise RuntimeError(f"failed to establish session for {user_id}")
        return session

    def send_as(self, user_id: str, payload: dict[str, Any]) -> dict[str, Any]:
        session = self.ensure_session(user_id)
        try:
            return session.send(payload)
        except RuntimeError:
            self.login(user_id)
            return self.ensure_session(user_id).send(payload)

    def disconnect(self, user_id: str) -> bool:
        with self._lock:
            session = self._sessions.pop(user_id, None)
        if session is None:
            return False
        session.close()
        return True

    def online_users(self) -> list[str]:
        with self._lock:
            items = list(self._sessions.items())
        return sorted([user_id for user_id, session in items if session.is_alive])


manager = GatewaySessionManager()


class LoginRequest(BaseModel):
    user_id: str = Field(min_length=1)


class DisconnectRequest(BaseModel):
    user_id: str = Field(min_length=1)


class MessageRequest(BaseModel):
    from_user: str = Field(min_length=1)
    to_user: str = Field(min_length=1)
    body: str = Field(min_length=1, max_length=4096)


class OfflinePullRequest(BaseModel):
    user_id: str = Field(min_length=1)


class FileManifestRequest(BaseModel):
    owner_user: str = Field(min_length=1)
    file_name: str = Field(min_length=1)
    file_size: int = Field(gt=0)
    chunk_size: int = Field(gt=0)
    total_chunks: int = Field(gt=0)
    sha256: str = ""
    transfer_id: str | None = None


class FileChunkRequest(BaseModel):
    user_id: str = Field(min_length=1)
    transfer_id: str = Field(min_length=1)
    chunk_index: int = Field(ge=0)
    data_base64: str = Field(min_length=1)
    eof: bool = False


class ResumeRequest(BaseModel):
    user_id: str = Field(min_length=1)
    transfer_id: str = Field(min_length=1)


class DownloadRequest(BaseModel):
    user_id: str = Field(min_length=1)
    transfer_id: str = Field(min_length=1)


app = FastAPI(title="dist-msg-platform bridge", version="0.1.0")
app.add_middleware(CORSMiddleware, allow_origins=["*"], allow_credentials=True, allow_methods=["*"], allow_headers=["*"])


def service_status_snapshot() -> dict[str, Any]:
    services = []
    for entry in SERVICE_PORTS:
        try:
            with socket.create_connection(("127.0.0.1", entry["port"]), timeout=0.3):
                healthy = True
        except OSError:
            healthy = False
        services.append({**entry, "healthy": healthy})
    return {"gateway_address": f"{GATEWAY_HOST}:{GATEWAY_PORT}", "services": services, "bridge": {"healthy": True, "port": 8080}, "connected_users": manager.online_users()}


def summary_snapshot() -> dict[str, Any]:
    try:
        response = send_frame_once({"cmd": "summary"})
        if response.get("ok"):
            return {"ok": True, "summary": {"online_users": response.get("online_users", 0), "stored_offline_messages": response.get("stored_offline_messages", 0), "registered_gateways": response.get("registered_gateways", 0), "active_transfers": response.get("active_transfers", 0)}}
        return {"ok": False, "message": response.get("message", "summary failed")}
    except Exception as exc:
        return {"ok": False, "message": str(exc), "summary": {"online_users": 0, "stored_offline_messages": 0, "registered_gateways": 0, "active_transfers": 0}}


@app.on_event("startup")
def startup_event() -> None:
    if DISABLE_POLLER:
        return

    def loop() -> None:
        while True:
            hub.broadcast({"type": "summary", **summary_snapshot()})
            hub.broadcast({"type": "services", **service_status_snapshot()})
            threading.Event().wait(SUMMARY_POLL_SECONDS)

    threading.Thread(target=loop, daemon=True).start()


@app.get("/api/summary")
def get_summary() -> dict[str, Any]:
    return summary_snapshot()


@app.get("/api/services")
def get_services() -> dict[str, Any]:
    return service_status_snapshot()


@app.post("/api/login")
def login(request: LoginRequest) -> dict[str, Any]:
    try:
        response = manager.login(request.user_id)
    except Exception as exc:
        raise HTTPException(status_code=400, detail=str(exc)) from exc
    payload = {"ok": True, "user": {"user_id": request.user_id, "session_id": response.get("session_id"), "gateway_instance_id": response.get("gateway_instance_id")}}
    hub.broadcast({"type": "login", **payload})
    return payload


@app.post("/api/disconnect")
def disconnect(request: DisconnectRequest) -> dict[str, Any]:
    disconnected = manager.disconnect(request.user_id)
    payload = {"ok": disconnected, "user_id": request.user_id}
    hub.broadcast({"type": "disconnect", **payload})
    return payload


@app.post("/api/send-message")
def send_message(request: MessageRequest) -> dict[str, Any]:
    try:
        response = manager.send_as(request.from_user, {"cmd": "send_message", "from_user": request.from_user, "to_user": request.to_user, "body": request.body})
    except Exception as exc:
        raise HTTPException(status_code=400, detail=str(exc)) from exc
    payload = {"ok": response.get("ok", False), "delivery": {"message_id": response.get("message_id"), "delivered_live": response.get("delivered_live", False), "stored_offline": response.get("stored_offline", False)}}
    hub.broadcast({"type": "send_message", "from_user": request.from_user, "to_user": request.to_user, "body": request.body, **payload})
    return payload


@app.post("/api/pull-offline")
def pull_offline(request: OfflinePullRequest) -> dict[str, Any]:
    try:
        response = manager.send_as(request.user_id, {"cmd": "pull_offline", "user_id": request.user_id})
    except Exception as exc:
        raise HTTPException(status_code=400, detail=str(exc)) from exc
    payload = {"ok": response.get("ok", False), "user_id": request.user_id, "messages": response.get("messages", [])}
    hub.broadcast({"type": "offline_batch", **payload})
    return payload


@app.post("/api/file/manifest")
def file_manifest(request: FileManifestRequest) -> dict[str, Any]:
    try:
        response = manager.send_as(request.owner_user, {"cmd": "file_manifest", "owner_user": request.owner_user, "file_name": request.file_name, "file_size": request.file_size, "chunk_size": request.chunk_size, "total_chunks": request.total_chunks, "sha256": request.sha256, **({"transfer_id": request.transfer_id} if request.transfer_id else {})})
    except Exception as exc:
        raise HTTPException(status_code=400, detail=str(exc)) from exc
    payload = {"ok": response.get("ok", False), "transfer": {"transfer_id": response.get("transfer_id"), "total_chunks": response.get("total_chunks", request.total_chunks), "file_name": request.file_name, "chunk_size": request.chunk_size, "sha256": request.sha256}}
    hub.broadcast({"type": "file_manifest", **payload})
    return payload


@app.post("/api/file/chunk")
def file_chunk(request: FileChunkRequest) -> dict[str, Any]:
    try:
        response = manager.send_as(request.user_id, {"cmd": "file_chunk", "transfer_id": request.transfer_id, "chunk_index": request.chunk_index, "data_base64": request.data_base64, "eof": request.eof})
    except Exception as exc:
        raise HTTPException(status_code=400, detail=str(exc)) from exc
    payload = {"ok": response.get("ok", False), "progress": {"transfer_id": response.get("transfer_id"), "chunk_index": response.get("chunk_index"), "received_chunks": response.get("received_chunks")}}
    hub.broadcast({"type": "file_chunk", **payload})
    return payload


@app.post("/api/file/resume")
def file_resume(request: ResumeRequest) -> dict[str, Any]:
    try:
        response = manager.send_as(request.user_id, {"cmd": "query_resume", "transfer_id": request.transfer_id})
    except Exception as exc:
        raise HTTPException(status_code=400, detail=str(exc)) from exc
    payload = {"ok": response.get("ok", False), "resume": {"transfer_id": response.get("transfer_id", request.transfer_id), "next_chunk": response.get("next_chunk", 0), "missing_chunks": response.get("missing_chunks", [])}}
    hub.broadcast({"type": "resume_token", **payload})
    return payload


@app.post("/api/file/download")
def file_download(request: DownloadRequest) -> dict[str, Any]:
    chunks: list[bytes] = []
    chunk_index = 0
    try:
        while chunk_index < 4096:
            response = manager.send_as(request.user_id, {"cmd": "download_chunk", "transfer_id": request.transfer_id, "chunk_index": chunk_index})
            data = base64.b64decode(response.get("data_base64", ""))
            chunks.append(data)
            if response.get("eof"):
                break
            chunk_index += 1
        else:
            raise RuntimeError("download exceeded chunk limit")
    except Exception as exc:
        raise HTTPException(status_code=400, detail=str(exc)) from exc
    content = b"".join(chunks)
    payload = {"ok": True, "file": {"transfer_id": request.transfer_id, "chunk_count": len(chunks), "byte_size": len(content), "data_base64": base64.b64encode(content).decode("ascii")}}
    hub.broadcast({"type": "file_download", **payload})
    return payload


@app.websocket("/ws/events")
async def websocket_events(websocket: WebSocket) -> None:
    await websocket.accept()
    subscriber = hub.subscribe()
    await websocket.send_json({"type": "hello", "message": "bridge connected", "timestamp": utc_now()})
    try:
        while True:
            event = await asyncio.to_thread(subscriber.get)
            await websocket.send_json(event)
    except WebSocketDisconnect:
        hub.unsubscribe(subscriber)
    except Exception:
        hub.unsubscribe(subscriber)
        raise