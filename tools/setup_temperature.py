"""Japanese USB setup for the Wi-Fi thermometer. No credential files or firmware writes."""
from __future__ import annotations

import argparse
import getpass
import json
import math
import time
import warnings
from typing import Any

ERRORS = {
    "invalid_request": "要求の形式が合いません。ツールと本体の版を確認してください。",
    "invalid_settings": "設定値を確認してください。保存されていません。",
    "storage_failed": "本体の設定保存に失敗しました。",
    "busy_retry": "現在送信中です。数秒待ってもう一度操作してください。",
    "not_ready": "Wi-Fi・時刻・本体の準備がまだ終わっていません。状態を確認してください。",
    "retry_wait": "送信間隔の待ち時間中です。状態で残り時間を確認してください。",
    "request_too_long": "設定データが長すぎます。",
}


class DeviceError(Exception):
    pass


class ResponseTimeout(DeviceError):
    pass


class DeviceClient:
    def __init__(self, connection: Any, timeout: float = 15.0):
        self.connection = connection
        self.timeout = timeout
        self.request_id = 0

    def request(self, command: str, settings: dict | None = None, *, timeout: float | None = None) -> dict:
        self.request_id += 1
        body: dict[str, Any] = {"cmd": command, "id": self.request_id}
        if settings is not None:
            body["settings"] = settings
        data = json.dumps(body, ensure_ascii=False, allow_nan=False, separators=(",", ":")).encode("utf-8")
        if len(data) > 1535:
            raise DeviceError(ERRORS["request_too_long"])
        self.connection.write(data + b"\n")
        deadline = time.monotonic() + (self.timeout if timeout is None else timeout)
        pending = bytearray()
        while time.monotonic() < deadline:
            fragment = self.connection.readline()
            if not fragment:
                continue
            pending.extend(fragment)
            if len(pending) > 8192:
                raise DeviceError("本体からの応答が長すぎます。")
            if not pending.endswith(b"\n"):
                continue
            try:
                response = json.loads(pending)
            except (ValueError, UnicodeDecodeError):
                pending.clear()
                continue  # Ignore boot diagnostics; never print raw serial content.
            pending.clear()
            if not isinstance(response, dict) or response.get("id") != self.request_id:
                continue
            if response.get("ok") is not True:
                error = response.get("error")
                raise DeviceError(ERRORS.get(error, "本体が操作を受け付けませんでした。") if isinstance(error, str) else "本体が操作を受け付けませんでした。")
            return response
        raise ResponseTimeout("本体の応答を待つ時間を超えました。接続と書き込まれた版を確認してください。")

    def identify(self) -> dict:
        # A newly opened USB connection can lose its first response. Retry only
        # this read-only identification, never configuration writes or messages.
        try:
            status = self.request("status", timeout=min(self.timeout, 3.0))
        except ResponseTimeout:
            status = self.request("status")
        version = status.get("firmware", "")
        if not isinstance(version, str) or not version.startswith("wifi-discord-"):
            raise DeviceError("Wi-Fi・Discord版の温度計ではありません。接続設定は送信していません。")
        return status


def valid_webhook(value: str) -> bool:
    prefixes = ("https://discord.com/api/webhooks/", "https://discord.com/api/v10/webhooks/")
    prefix = next((p for p in prefixes if value.startswith(p)), None)
    if prefix is None:
        return False
    parts = value[len(prefix):].split("/")
    return len(parts) == 2 and 16 <= len(parts[0]) <= 21 and parts[0].isascii() and parts[0].isdigit() and \
        30 <= len(parts[1]) <= 150 and all(c.isascii() and (c.isalnum() or c in "_-.") for c in parts[1]) and \
        len(value.encode("utf-8")) < 256


def secret_input(prompt: str) -> str:
    try:
        with warnings.catch_warnings():
            warnings.simplefilter("error", getpass.GetPassWarning)
            return getpass.getpass(prompt)
    except getpass.GetPassWarning:
        raise DeviceError("非表示で入力できる端末が必要です。PCのPowerShellから設定ツールを開いてください。") from None


def read_text(prompt: str, maximum: int, *, secret: bool = False, minimum: int = 1) -> str:
    while True:
        value = secret_input(prompt) if secret else input(prompt)
        length = len(value.encode("utf-8"))
        if minimum <= length <= maximum and not any(ord(c) < 32 or ord(c) == 127 for c in value):
            return value
        print(f"長さは{minimum}〜{maximum}バイトです。日本語は通常1文字3バイトです。")


def number(prompt: str, default: float, minimum: float, maximum: float, *, integer: bool = False) -> float | int:
    while True:
        text = input(f"{prompt} [{default:g}]: ").strip()
        try:
            value = float(text) if text else float(default)
        except ValueError:
            value = math.nan
        if math.isfinite(value) and minimum <= value <= maximum and (not integer or value.is_integer()):
            return int(value) if integer else value
        print(f"{minimum:g}〜{maximum:g}の{'整数' if integer else '数値'}を入力してください。")


def yes_no(prompt: str, default: bool) -> bool:
    while True:
        value = input(f"{prompt} (y/n) [{'y' if default else 'n'}]: ").strip().lower()
        if not value:
            return default
        if value in ("y", "yes"):
            return True
        if value in ("n", "no"):
            return False
        print("y または n を入力してください。")


def show_status(status: dict) -> None:
    clock = {"ntp": "ネット時刻", "rtc": "本体時計", "none": "時刻未取得"}.get(status.get("clock"), "未確認")
    print(f"本体の版: {status.get('firmware', '未確認')}")
    print("接続設定: " + ("保存済み" if status.get("configured") else "未設定"))
    print("Wi-Fi: " + ("接続中" if status.get("wifi_connected") else "未接続") + " / " + clock)
    if status.get("reading_valid"):
        print(f"温度 {status['temperature']:.1f}℃ / 湿度 {status['humidity']:.1f}%")
    else:
        print("温湿度: まだ取得できていません")
    print(f"保存する履歴: {status.get('history_points', 0)}点 / 保存状態: " + ("正常" if status.get("storage_ok") else "要確認"))
    code = status.get("http_status", 0)
    labels = {0: "送信成功の記録なし", 200: "送信成功", 429: "Discordの待ち時間中", 400: "内容または設定エラー",
              401: "接続先の認証エラー", 403: "接続先の権限エラー", 404: "Webhookが見つかりません"}
    label = "通信・TLSの接続に失敗" if code == 0 and status.get("send_attempted") else labels.get(code, f"HTTP {code}")
    print("送信: " + label)
    if status.get("send_blocked"):
        print("自動送信を停止しています。Webhookを確認・再設定してください。")
    if status.get("retry_seconds", 0):
        print(f"次の送信までの待ち時間: 約{status['retry_seconds']:.0f}秒")
    if not status.get("worker_ready", True):
        print("通信処理の起動に失敗しています。本体を再起動してください。")


def configure_connection(client: DeviceClient) -> None:
    print("Wi-Fiは2.4GHzを使用します。パスワードとWebhook URLは画面に表示せず、ファイルにも保存しません。")
    ssid = read_text("Wi-Fiの名前（SSID）: ", 32)
    password = read_text("Wi-Fiパスワード（表示されません。暗号化なしのWi-Fiなら空欄）: ", 63, secret=True, minimum=0)
    while password and len(password.encode("utf-8")) < 8:
        print("パスワードは8〜63バイトです。")
        password = read_text("Wi-Fiパスワード: ", 63, secret=True, minimum=0)
    while True:
        url = secret_input("DiscordのWebhook URL（表示されません）: ").strip()
        if valid_webhook(url):
            break
        print("Discordからコピーした https://discord.com/api/webhooks/... のURLを入力してください。")
    client.request("configure", {"ssid": ssid, "wifi_password": password, "webhook_url": url})
    print("接続設定を本体へ保存しました。接続と時刻取得を待ち、状態を確認してください。")


def configure_behavior(client: DeviceClient) -> None:
    settings = client.request("status")["settings"]
    print("空欄は現在の値を使います。これらの条件は好みに合わせる設定です。")
    updates = {
        "report_minutes": number("定期通知の間隔（分、0で停止）", settings["report_minutes"], 0, 1440, integer=True),
        "temp_enabled": yes_no("高温を通知する", settings["temp_enabled"]),
        "temp_high": number("高温とする温度（℃）", settings["temp_high"], -40, 85),
        "humidity_enabled": yes_no("高湿度を通知する", settings["humidity_enabled"]),
        "humidity_high": number("高湿度とする湿度（%）", settings["humidity_high"], 0, 100),
        "hold_seconds": number("条件が続いてから通知するまで（秒）", settings["hold_seconds"], 10, 3600, integer=True),
        "cooldown_minutes": number("異常が続くときの再通知・ブザー間隔（分）", settings["cooldown_minutes"], 1, 1440, integer=True),
        "temp_hysteresis": number("温度が何℃下がれば回復とするか", settings["temp_hysteresis"], 0.1, 10),
        "humidity_hysteresis": number("湿度が何%下がれば回復とするか", settings["humidity_hysteresis"], 0.1, 20),
        "buzzer_enabled": yes_no("本体ブザーを使う", settings["buzzer_enabled"]),
        "screen_always_on": yes_no("画面を常時表示する", settings["screen_always_on"]),
        "sample_seconds": number("温湿度を測る間隔（秒）", settings["sample_seconds"], 10, 300, integer=True),
        "save_minutes": number("履歴の保存間隔（分）", settings["save_minutes"], 1, 120, integer=True),
    }
    if yes_no("通知に表示する本体名を変更する", False):
        updates["name"] = read_text("本体名: ", 63)
    client.request("configure", updates)
    print("本体へ設定を保存しました。異常の継続時間の判定はここからやり直します。")


def main() -> int:
    parser = argparse.ArgumentParser(description="Wi-Fi温湿度計のUSB設定")
    parser.add_argument("--port", help="COM4などの接続ポート。秘密値はコマンド引数へ渡しません。")
    parser.add_argument("--status", action="store_true", help="状態だけ確認して終了")
    args = parser.parse_args()
    try:
        import serial
        from serial.tools import list_ports
    except ImportError:
        print("pyserialが必要です。開発手順のPC設定ツールの準備を確認してください。")
        return 1
    try:
        port = args.port
        if not port:
            ports = list(list_ports.comports())
            if not ports:
                raise DeviceError("USBポートが見つかりません。XIAO本体とPCをデータ対応ケーブルで接続してください。")
            for i, item in enumerate(ports, 1):
                print(f"{i}: {item.device} ({item.description})")
            choice = number("接続する番号", 1, 1, len(ports), integer=True)
            port = ports[int(choice) - 1].device
        connection = serial.Serial(port=None, baudrate=115200, timeout=0.2, write_timeout=2)
        connection.dtr = False
        connection.rts = False
        connection.port = port
        connection.open()
        with connection:
            client = DeviceClient(connection)
            show_status(client.identify())
            if args.status:
                return 0
            while True:
                print("\n1: Wi-Fi・Webhook設定  2: 通知・画面・ブザー設定  3: 状態確認\n4: Discordへ1回送信して確認  5: 接続設定だけを消去  0: 終了")
                choice = input("番号: ").strip()
                try:
                    if choice == "0":
                        break
                    if choice == "1":
                        configure_connection(client)
                    elif choice == "2":
                        configure_behavior(client)
                    elif choice == "3":
                        show_status(client.request("status"))
                    elif choice == "4":
                        client.request("test")
                        print("1回の送信を開始しました。数秒後に状態とDiscordの受信を確認してください。")
                    elif choice == "5":
                        client.request("clear_connection")
                        print("新版のWi-Fi・Webhook設定を消去しました。履歴と表示設定は保持しています。")
                except DeviceError as error:
                    print(str(error))
    except DeviceError as error:
        print(str(error))
        return 1
    except (serial.SerialException, OSError):
        # Serial exceptions can contain private device paths. Do not dump requests/tracebacks.
        print("接続・応答を確認できませんでした。ケーブル、ポート、Wi-Fi版の書き込みを確認してください。")
        return 1
    except (KeyboardInterrupt, EOFError):
        print("\n設定操作を終了しました。")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
