#!/usr/bin/env python3
import argparse
import json
import socket
import struct


def send_frame(sock: socket.socket, payload: dict) -> None:
    data = json.dumps(payload).encode("utf-8")
    sock.sendall(struct.pack("!I", len(data)) + data)


def recv_frame(sock: socket.socket) -> dict:
    header = sock.recv(4)
    if len(header) != 4:
        raise RuntimeError("connection closed")
    length = struct.unpack("!I", header)[0]
    payload = b""
    while len(payload) < length:
        chunk = sock.recv(length - len(payload))
        if not chunk:
            raise RuntimeError("connection closed")
        payload += chunk
    return json.loads(payload.decode("utf-8"))


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=7001)
    parser.add_argument("command")
    args = parser.parse_args()

    with socket.create_connection((args.host, args.port), timeout=5) as sock:
        send_frame(sock, json.loads(args.command))
        print(json.dumps(recv_frame(sock), indent=2, ensure_ascii=False))


if __name__ == "__main__":
    main()