import os
import unittest
from unittest.mock import patch

os.environ["DIST_PLATFORM_DISABLE_POLLER"] = "1"

from fastapi.testclient import TestClient

import bridge.app as bridge_app


class FakeManager:
    def login(self, user_id):
        return {"ok": True, "session_id": f"session-{user_id}", "gateway_instance_id": "gateway-1"}

    def disconnect(self, user_id):
        return True

    def send_as(self, user_id, payload):
        cmd = payload["cmd"]
        if cmd == "send_message":
            return {"ok": True, "message_id": "msg-1", "delivered_live": True, "stored_offline": False}
        if cmd == "pull_offline":
            return {"ok": True, "messages": [{"body": "offline hello", "from_user": "alice", "to_user": user_id}]}
        return {"ok": True}

    def online_users(self):
        return ["alice", "bob"]


class BridgeApiTests(unittest.TestCase):
    def setUp(self):
        self.manager_patch = patch.object(bridge_app, "manager", FakeManager())
        self.summary_patch = patch.object(
            bridge_app,
            "summary_snapshot",
            lambda: {"ok": True, "summary": {"online_users": 2, "stored_offline_messages": 0, "registered_gateways": 1, "active_transfers": 0}},
        )
        self.services_patch = patch.object(
            bridge_app,
            "service_status_snapshot",
            lambda: {"gateway_address": "127.0.0.1:7001", "services": [{"name": "gateway_service", "port": 7001, "label": "Gateway", "healthy": True}], "bridge": {"healthy": True, "port": 8080}, "connected_users": ["alice", "bob"]},
        )
        self.manager_patch.start()
        self.summary_patch.start()
        self.services_patch.start()
        self.client = TestClient(bridge_app.app)

    def tearDown(self):
        self.manager_patch.stop()
        self.summary_patch.stop()
        self.services_patch.stop()

    def test_summary_endpoint(self):
        response = self.client.get("/api/summary")
        self.assertEqual(response.status_code, 200)
        self.assertEqual(response.json()["summary"]["registered_gateways"], 1)

    def test_send_message_endpoint(self):
        response = self.client.post("/api/send-message", json={"from_user": "alice", "to_user": "bob", "body": "hello"})
        self.assertEqual(response.status_code, 200)
        self.assertTrue(response.json()["delivery"]["delivered_live"])

    def test_services_endpoint(self):
        response = self.client.get("/api/services")
        self.assertEqual(response.status_code, 200)
        self.assertEqual(response.json()["gateway_address"], "127.0.0.1:7001")


if __name__ == "__main__":
    unittest.main()