# Wi-Fi・Discord版の検証記録

2026-10-04。対象はmcu_firmware/xiao_env_wifi_discord/とtools/setup_temperature.py。実装前の理由は[設計判断](DECISIONS.md)D009・D010、操作は[使い方](WIFI_DISCORD.md)に記載。

## GitHubでの変更記録

[PR #114](https://github.com/kakinymax/xiao-scanner/pull/114)に実装をまとめた。製品コードとテストの識別番号は[446c82189b61fbff485d45b97b1f8acb968eb4a9](https://github.com/kakinymax/xiao-scanner/commit/446c82189b61fbff485d45b97b1f8acb968eb4a9)。GitHubから28ファイルを取得し直し、PCのソースと一致することを確認した。後続の文書追記はこのコードを変更しない。同日、同じ実機へこのコードのwifi-discord-1.0.0を書き込み、四つの書き込み領域のハッシュ一致を確認した。通常起動と使用環境での受入は別に確認する。

## PCで完了した確認

| 対象 | 方法・結果 | 証明する範囲 |
|---|---|---|
| XIAO ESP32C3向けビルド | Arduino CLI 1.5.1、ESP32 core 3.3.0、既定XIAO_ESP32C3で成功。プログラム1,244,296 / 1,310,720バイト、静的RAM43,992 / 327,680バイト | 新スケッチとU8g2/RTClib・WiFi/HTTPS/cJSON/証明書束が対象ボード向けにコンパイル・リンクでき、既定領域へ収まること |
| 計測・通知・履歴・設定のC++ | tests/firmware/core_test.cppで6シナリオ群・733件の値と条件を確認、成功。同じthermo_core.hとdevice_config.hをそのままコンパイルして実行 | 下記のロジック・形式・境界。実センサー・電源断・Wi-Fi・Discordの再現ではない |
| PC設定ツール | Python標準unittest、7件成功 | 応答分割、起動メッセージ・別要求の除外、旧版への設定防止、エラーや状態に秘密値を表示しないこと、時間切れ・長い要求、URL制限、秘密入力の非表示不能時の停止 |
| 起動コマンド | tools/setup_temperature.ps1 --helpが成功。pyserial 3.5を準備し、--statusでポート検出まで起動。確認時点のUSBポートは0件 | このWindows環境で依存関係を読み込み設定ツールを起動できること。実USB設定成功ではない |

最初のC++確認はWindowsネイティブ形式で608件成功。履歴の区切りを追加して生成し直したexeはWindowsのアプリケーション制御により実行不可だったため、**OSの設定を変更せず**同じテストをWASI形式へコンパイルし、既存Node.js 24.19.0で733件を実行した。Zig 0.17.0は公式配布ハッシュを確認して作業用toolsへ配置。cJSONは本体付属と同じ1.7.18の公式ソースをPC検証に使用。NodeのWASIは実験的APIの警告が出るが、本番ファームウェアの依存ではない。新しいテストフレームワーク・定期実行は導入していない。

## C++の確認内容

- AHT20の変換（既知の25℃・50%応答）、CRC8、途中までの応答、BUSY、未校正、無効な値。
- 30℃・80%の境界、2分継続、しきい値周辺の揺れ、1℃・5%の戻り幅、全復帰・一部復帰、センサー不応答、判定停止、49日を超える稼働時間。
- 起動・定期・継続・復帰・状態変化の通知、送信中の変化、通信断中の変化を最新状態にまとめること。
- wait=trueの200を成功と扱うこと、429の秒・小数秒の待ち、サーバー待ち時間と設定変更後の保持、3回失敗後の長い待機、無効なWebhookでの自動停止。
- 120点の循環、各点の温湿度と日時の保存・復元、欠けた保存・CRC不一致・正しいCRCでも不正値の場合の拒否、世代番号の周回、時刻修正後の未来データ、24時間グラフの位置。
- 短い電源断の前後を線で結ばない区切りを保存し、復元できること。12分点の間隔が大きく空く区間も接続しないこと。
- 部分的な設定更新、未知・重複項目、型・範囲・空の本体名・制御文字の拒否、不正設定が途中まで反映されないこと、Discordの正規URL制限、状態応答にSSID・パスワード・Webhookを含めないこと。

## 再実行するコマンド

プロジェクト直下で実行する。別PCでは各実行パスを準備したものに読み替える。コンパイルはUSB書き込みを行わない。

```powershell
$env:ARDUINO_DIRECTORIES_DATA = 'C:\Users\pc\Documents\Codex\mcu'
& 'C:\Users\pc\Documents\Codex\tools\temperature\arduino-cli\1.5.1\arduino-cli.exe' --config-file 'C:\Users\pc\Documents\Codex\tools\temperature\arduino-cli.yaml' compile --fqbn esp32:esp32:XIAO_ESP32C3 --warnings all --build-path build/wifi-discord mcu_firmware/xiao_env_wifi_discord
& 'C:\Users\pc\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe' -X utf8 -m unittest discover -s tests/firmware -p test_setup_temperature.py -v

$zigExe = 'C:\Users\pc\Documents\Codex\tools\temperature\zig\zig-x86_64-windows-0.17.0\zig.exe'
$cjsonSource = 'C:\Users\pc\Documents\Codex\tools\temperature\host-test-cjson\1.7.18'
$env:ZIG_GLOBAL_CACHE_DIR = 'C:\Users\pc\Documents\Codex\tools\temperature\zig-cache'
New-Item -ItemType Directory -Force -Path build/host-tests | Out-Null
& $zigExe cc -target wasm32-wasi -O2 -DCJSON_HIDE_SYMBOLS -c "$cjsonSource/cJSON.c" -o build/host-tests/cJSON-wasi.o
if ($LASTEXITCODE -ne 0) { throw 'cJSON compile failed' }
& $zigExe c++ -target wasm32-wasi -std=c++17 -Wall -Wextra -Werror -DCJSON_HIDE_SYMBOLS -I mcu_firmware/xiao_env_wifi_discord -I $cjsonSource tests/firmware/core_test.cpp build/host-tests/cJSON-wasi.o -o build/host-tests/core_test.wasm
if ($LASTEXITCODE -ne 0) { throw 'C++ compile failed' }
node tests/firmware/run_wasi.cjs build/host-tests/core_test.wasm
if ($LASTEXITCODE -ne 0) { throw 'C++ tests failed' }
```

WASIの実行対象はこのプロジェクトの自作テストだけ。未知のプログラムを安全に実行する仕組みとして使わない。NodeのWASIについては[公式説明](https://nodejs.org/docs/latest-v24.x/api/wasi.html)を参照。

## 実機で未確認の項目

同じ実機の追加データ保全と、検証済みwifi-discord-1.0.0のUSB書き込みを完了した。[実機切替の記録](DEVICE_WIFI_CUTOVER.md)を参照。四つの領域は書き込み後のハッシュ照合に成功したが、初回のUSB状態確認は時間切れ。ユーザーのRESET操作後にwifi-discord-1.0.0のUSB応答、保存状態正常、通信処理の起動を確認した。XIAOはベースから外れていたため、温湿度・時計・画面は取り付け後に確認する。実Webhook送信・旧Web公開の停止は未実施。[共同対応票](WIFI_DISCORD_JOINT_ACTIONS.md)に従い、起動、接続先の入力、受信、ボタン・表示・ブザー、通信断と復帰、再起動と保存、センサー断と復帰を確認する。

HTTPSの証明書検証・リダイレクト禁止・待ち時間と別タスク送信は実装・ビルドを確認済み。使用環境でのDNS/TLS・NTP・Wi-Fi・Discord成功、実際のフラッシュ書き込み中の電源断耐性、長時間のメモリ使用はまだ確認していない。静的RAM値はTLSタスク等の実行中の最大使用量ではない。PCテストの成功を実機の受入完了として扱わない。
