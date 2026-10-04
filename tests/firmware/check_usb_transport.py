"""Read-only USB regression checks on the real Wi-Fi thermometer.

Requires pyserial. Sends status requests only; never changes configuration or flash.
Prints packet sizes and pass/fail, without raw serial data or connection settings.
"""
from __future__ import annotations

import argparse
import json
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
from setup_temperature import DeviceClient, DeviceError


def padded_settings(client: DeviceClient, size: int, unicode: bool = False) -> dict:
    settings = {"probe_padding": ""}
    body = {"cmd": "status", "id": client.request_id + 1, "settings": settings}
    remaining = size - len(json.dumps(body, ensure_ascii=False, separators=(",", ":")).encode("utf-8"))
    if remaining < 0:
        raise ValueError("Target packet size is too small")
    settings["probe_padding"] = "温" * (remaining // 3) + "x" * (remaining % 3) if unicode else "x" * remaining
    return settings


def connection(port: str):
    import serial

    link = serial.Serial(port=None, baudrate=115200, timeout=0.25, write_timeout=2)
    link.dtr = False
    link.rts = False
    link.port = port
    link.open()
    return link


def run(port: str) -> dict:
    checks = []
    with connection(port) as link:
        client = DeviceClient(link, timeout=5)
        initial = client.identify()
        firmware = initial["firmware"]
        for round_number in range(1, 4):
            for size in (64, 256, 512, 1024, 1535):
                response = client.request("status", padded_settings(client, size))
                assert response["firmware"] == firmware
                assert response["configured"] == initial["configured"]
                assert response["settings"] == initial["settings"]
                checks.append({"kind": "status", "round": round_number, "json_bytes": size, "ok": True})
        response = client.request("status", padded_settings(client, 1535, unicode=True))
        assert response["firmware"] == firmware
        checks.append({"kind": "utf8_status", "json_bytes": 1535, "ok": True})

        # Exercise the firmware's length guard, bypassing the client's own guard.
        body = {"cmd": "status", "id": client.request_id + 1, "settings": padded_settings(client, 1536)}
        link.write(json.dumps(body, separators=(",", ":")).encode("utf-8") + b"\n")
        deadline = time.monotonic() + 5
        line = bytearray()
        while time.monotonic() < deadline and not line.endswith(b"\n"):
            line.extend(link.readline())
        reply = json.loads(line)
        assert reply.get("ok") is False and reply.get("error") == "request_too_long"
        checks.append({"kind": "over_limit_rejected", "json_bytes": 1536, "ok": True})
        final = client.identify()
        assert final["configured"] == initial["configured"] and final["settings"] == initial["settings"]
        checks.append({"kind": "status_after_rejection", "ok": True})

    for iteration in range(1, 4):
        with connection(port) as link:
            response = DeviceClient(link, timeout=5).identify()
            assert response["firmware"] == firmware
            checks.append({"kind": "reopen", "iteration": iteration, "ok": True})
    return {"firmware": firmware, "configuration_changed": False, "checks": checks}


def main() -> int:
    parser = argparse.ArgumentParser(description="温度計のUSB通信を状態要求だけで確認します。設定・書き込みは変更しません。")
    parser.add_argument("--port", required=True)
    args = parser.parse_args()
    try:
        result = run(args.port)
    except (DeviceError, AssertionError, ValueError, OSError):
        print(json.dumps({"ok": False, "error": "USB transport verification failed; no settings or flash were changed."}))
        return 1
    print(json.dumps(result, ensure_ascii=False, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
