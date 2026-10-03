import contextlib
import importlib.util
import io
import json
from pathlib import Path
import unittest
from unittest.mock import patch
import warnings

path = Path(__file__).resolve().parents[2] / "tools" / "setup_temperature.py"
spec = importlib.util.spec_from_file_location("setup_temperature", path)
setup = importlib.util.module_from_spec(spec)
spec.loader.exec_module(setup)


class FakeSerial:
    def __init__(self, fragments):
        self.fragments = list(fragments)
        self.writes = []

    def write(self, data):
        self.writes.append(data)

    def readline(self):
        return self.fragments.pop(0) if self.fragments else b""


class SetupTests(unittest.TestCase):
    def test_partial_lines_boot_text_and_unrelated_replies(self):
        port = FakeSerial([b"boot ready\n", b'{"id":99,"ok":true}\n', b'{"id":1,', b'"ok":true,"firmware":"wifi-discord-1.0.0"}\n'])
        reply = setup.DeviceClient(port).identify()
        self.assertEqual(reply["firmware"], "wifi-discord-1.0.0")
        self.assertEqual(json.loads(port.writes[0]), {"cmd": "status", "id": 1})

    def test_errors_never_echo_device_input(self):
        port = FakeSerial([b'{"id":1,"ok":false,"error":"PRIVATE_PASSWORD"}\n'])
        with self.assertRaises(setup.DeviceError) as error:
            setup.DeviceClient(port).request("status")
        self.assertNotIn("PRIVATE_PASSWORD", str(error.exception))

    def test_wrong_firmware_does_not_receive_credentials(self):
        port = FakeSerial([b'{"id":1,"ok":true,"firmware":"old-collector"}\n'])
        with self.assertRaises(setup.DeviceError):
            setup.DeviceClient(port).identify()
        self.assertEqual(len(port.writes), 1)
        self.assertEqual(json.loads(port.writes[0])["cmd"], "status")

    def test_timeout_and_oversized_request(self):
        port = FakeSerial([])
        with self.assertRaises(setup.DeviceError):
            setup.DeviceClient(port, timeout=0.001).request("status")
        with self.assertRaises(setup.DeviceError):
            setup.DeviceClient(port).request("configure", {"name": "x" * 2000})
        self.assertEqual(len(port.writes), 1)

    def test_webhook_host_query_and_token_validation(self):
        value = "https://discord.com/api/webhooks/123456789012345678/PLACEHOLDER_NOT_A_REAL_TOKEN_123456"
        self.assertTrue(setup.valid_webhook(value))
        self.assertFalse(setup.valid_webhook(value.replace("discord.com", "discord.com.evil.example")))
        self.assertFalse(setup.valid_webhook(value + "?wait=false"))
        self.assertFalse(setup.valid_webhook(value.replace("https://", "http://")))
        self.assertFalse(setup.valid_webhook(value + "/extra"))

    def test_status_output_uses_public_fields_only(self):
        stream = io.StringIO()
        with contextlib.redirect_stdout(stream):
            setup.show_status({"firmware": "wifi-discord-1.0.0", "clock": "ntp", "configured": True,
                "wifi_connected": True, "reading_valid": True, "temperature": 25.2, "humidity": 55,
                "history_points": 120, "storage_ok": True, "http_status": 200,
                "settings": {"wifi_password": "PRIVATE_PASSWORD", "webhook_url": "PRIVATE_URL"}})
        text = stream.getvalue()
        self.assertIn("25.2", text)
        self.assertIn("送信成功", text)
        self.assertNotIn("PRIVATE_PASSWORD", text)
        self.assertNotIn("PRIVATE_URL", text)

    def test_secret_prompt_refuses_echo_fallback(self):
        def fallback(prompt):
            warnings.warn("Cannot disable echo", setup.getpass.GetPassWarning)
            self.fail("Should stop before requesting an echoed secret")
        with patch.object(setup.getpass, "getpass", side_effect=fallback):
            with self.assertRaises(setup.DeviceError):
                setup.secret_input("secret: ")


if __name__ == "__main__":
    unittest.main()
