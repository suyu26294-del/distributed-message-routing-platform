#!/usr/bin/env python3
import argparse
import base64
import hashlib
import json
import os
import socket
import struct


class JsonClient:
    def __init__(self, host: str, port: int):
        self.sock = socket.create_connection((host, port), timeout=5)

    def close(self) -> None:
        self.sock.close()

    def send(self, payload: dict) -> dict:
        data = json.dumps(payload).encode("utf-8")
        self.sock.sendall(struct.pack("!I", len(data)) + data)
        return self.recv()

    def recv(self) -> dict:
        header = self.sock.recv(4)
        if len(header) != 4:
            raise RuntimeError("connection closed")
        length = struct.unpack("!I", header)[0]
        payload = b""
        while len(payload) < length:
            chunk = self.sock.recv(length - len(payload))
            if not chunk:
                raise RuntimeError("connection closed")
            payload += chunk
        return json.loads(payload.decode("utf-8"))


def assert_true(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=7001)
    args = parser.parse_args()

    alice = JsonClient(args.host, args.port)
    bob = JsonClient(args.host, args.port)

    try:
        login_alice = alice.send({"cmd": "login", "user_id": "alice"})
        login_bob = bob.send({"cmd": "login", "user_id": "bob"})
        assert_true(login_alice["ok"] and login_bob["ok"], "logins failed")

        send_live = alice.send({"cmd": "send_message", "from_user": "alice", "to_user": "bob", "body": "hello live"})
        live_push = bob.recv()
        assert_true(send_live["delivered_live"], "live message was not delivered immediately")
        assert_true(live_push["body"] == "hello live", "bob received unexpected live body")

        bob.close()
        bob = None
        send_offline = alice.send({"cmd": "send_message", "from_user": "alice", "to_user": "bob", "body": "hello offline"})
        assert_true(send_offline["stored_offline"], "offline message was not stored")

        bob = JsonClient(args.host, args.port)
        login_bob_again = bob.send({"cmd": "login", "user_id": "bob"})
        assert_true(login_bob_again["ok"], "bob relogin failed")
        offline_batch = bob.send({"cmd": "pull_offline", "user_id": "bob"})
        assert_true(len(offline_batch["messages"]) == 1, "expected one offline message")
        assert_true(offline_batch["messages"][0]["body"] == "hello offline", "unexpected offline body")

        file_bytes = (b"dist-msg-platform-demo-" * 7000)[:180000]
        chunk_size = 65536
        total_chunks = (len(file_bytes) + chunk_size - 1) // chunk_size
        digest = hashlib.sha256(file_bytes).hexdigest()

        manifest_response = alice.send(
            {
                "cmd": "file_manifest",
                "owner_user": "alice",
                "file_name": "demo.bin",
                "file_size": len(file_bytes),
                "chunk_size": chunk_size,
                "total_chunks": total_chunks,
                "sha256": digest,
            }
        )
        transfer_id = manifest_response["transfer_id"]

        first_chunk = file_bytes[:chunk_size]
        upload_first = alice.send(
            {
                "cmd": "file_chunk",
                "transfer_id": transfer_id,
                "chunk_index": 0,
                "data_base64": base64.b64encode(first_chunk).decode("ascii"),
                "eof": False,
            }
        )
        assert_true(upload_first["ok"], "first chunk upload failed")

        resume = alice.send({"cmd": "query_resume", "transfer_id": transfer_id})
        assert_true(resume["missing_chunks"] == list(range(1, total_chunks)), "resume token mismatch after interruption")

        for index in range(1, total_chunks):
          start = index * chunk_size
          end = min(len(file_bytes), (index + 1) * chunk_size)
          response = alice.send(
              {
                  "cmd": "file_chunk",
                  "transfer_id": transfer_id,
                  "chunk_index": index,
                  "data_base64": base64.b64encode(file_bytes[start:end]).decode("ascii"),
                  "eof": index + 1 == total_chunks,
              }
          )
          assert_true(response["ok"], f"chunk {index} upload failed")

        downloaded = bytearray()
        for index in range(total_chunks):
            chunk = alice.send({"cmd": "download_chunk", "transfer_id": transfer_id, "chunk_index": index})
            downloaded.extend(base64.b64decode(chunk["data_base64"]))

        assert_true(bytes(downloaded) == file_bytes, "downloaded file bytes do not match uploaded bytes")

        summary = alice.send({"cmd": "summary"})
        assert_true(summary["ok"], "summary failed")

        result = {
            "message_demo": {
                "live": send_live,
                "offline": offline_batch,
            },
            "file_demo": {
                "transfer_id": transfer_id,
                "sha256": digest,
                "bytes": len(file_bytes),
                "chunks": total_chunks,
            },
            "summary": summary,
        }
        print(json.dumps(result, indent=2, ensure_ascii=False))
    finally:
        alice.close()
        if bob is not None:
            bob.close()


if __name__ == "__main__":
    main()