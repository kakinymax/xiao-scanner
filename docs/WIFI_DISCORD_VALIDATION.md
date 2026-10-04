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

## 初回切替の確認と残る受入

同じ実機の追加データ保全と、検証済みwifi-discord-1.0.0のUSB書き込みを完了した。[実機切替の記録](DEVICE_WIFI_CUTOVER.md)を参照。四つの領域は書き込み後のハッシュ照合に成功したが、初回のUSB状態確認は時間切れ。ユーザーのRESET操作後にwifi-discord-1.0.0のUSB応答、保存状態正常、通信処理の起動を確認した。XIAOはベースから外れていたため、初回切替時には温湿度・時計・画面と実Webhook送信が未確認だった。その後のPC接続でWi-Fi/NTP・HTTP200と本人の試験受信、再起動後の接続設定保持を確認した。その後、本体表示・画面/期間切替・実測起動通知・通常の電源断後の履歴復元を本人が確認した。現在の未確認は異常/復帰・ブザー/障害時動作と試験設定の復元で、旧Web公開の停止も受入後に行う。[共同対応票](WIFI_DISCORD_JOINT_ACTIONS.md)に従い、起動、接続先の入力、受信、ボタン・表示・ブザー、通信断と復帰、再起動と保存、センサー断と復帰を確認する。

HTTPSの証明書検証・リダイレクト禁止・待ち時間と別タスク送信は実装・ビルドを確認済み。その後のPC接続で使用環境のWi-Fi/NTPとDiscord HTTPSのHTTP200を確認した。実際のフラッシュ書き込み中の電源断耐性、長時間のメモリ使用はまだ確認していない。静的RAM値はTLSタスク等の実行中の最大使用量ではない。PCテストの成功を実機の受入完了として扱わない。

## USB通信修正版1.0.1の追加確認

1.0.0の状態要求は短い場合に成功したが、長い要求では応答がなく、短い要求の応答でもJSONの欠けを確認した。判断D013に従ってHWCDCの送受信容量を各256から2048バイトへ増やした。要求1535バイト上限と各ループ128バイトの処理制限は維持。通知・履歴・設定の意味は変更しない。

XIAO ESP32C3向けコンパイル成功（プログラム1,244,326バイト、静的RAM43,992バイト）。C++733件/6群とPCツール7件を再実行して成功。増えた動的バッファ3584バイトと管理領域は静的RAM値には含まれない。[切替記録](DEVICE_WIFI_CUTOVER.md)の通り、app0のみを書き込み、ハッシュ一致と通常起動を確認した。

`tests/firmware/check_usb_transport.py`は実機へstatusだけを送り、21項目で成功した。64/256/512/1024/1535バイトの要求各3回、最大長のUTF-8、1536バイト要求の拒否と次の状態応答、再オープン3回。開始と終了の公開設定を比較し、接続設定は変わっていない。USB要求が欠ける症状の改善を示し、Wi-Fi/Discord・センサー/ベースの受入完了は示さない。

```powershell
$env:PYTHONPATH = 'C:\Users\pc\Documents\Codex\tools\temperature\python-libs'
& 'C:\Users\pc\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe' -X utf8 tests/firmware/check_usb_transport.py --port COM4
```

設定ツールがポートを使用している間は実行しない。pyserial 3.5を使用し、DTR/RTSを解除してから開く。生の応答や接続情報を印字せず、パケットサイズと成否のみを表示する。

USB修正と実機検査は[PR #117](https://github.com/kakinymax/xiao-scanner/pull/117)、製品コード・検査スクリプトSHA 180443590ba37d99cfa20400642047b1cc80bbcd。GitHubから初回13ファイルの取得照合を完了。文書の更新コミットと実機のプログラム版は区別する。

## PC版確認の再試行と実設定後の確認

判断D014により、版を確認する初回statusだけ、時間切れなら一度待ち直す。最初の待ち上限3秒、次は通常の上限15秒。要求IDを変え、遅れた前の応答は除外する。configure/clear_connection/testの書き込み・送信を自動再試行しない。PCテストは2件を追加した計9件で成功。[PR #117](https://github.com/kakinymax/xiao-scanner/pull/117)、PCツール/テストSHA 56a4c214cdf517afe73749538ad85fe7285be014の2ファイルを取得照合した。ファームウェアは1.0.1から変更していない。

本人が接続先を直接入力し、Wi-Fi/NTP・Discord試験受信を確認。PC公開状態で設定保存・接続・NTP・保存正常・HTTP200を確認した。USBを保った再起動後も公開設定を保持し、保存した接続設定でWi-Fi/NTP・HTTP200へ復帰した。初回の状態要求が応答しなかった観察と、その後の成功を残す。センサー未接続で測定不能の通知を使う段階のため、実測値での定期・異常/復帰、グラフ・ブザー、履歴復元と障害時動作は最後のベース受入で確認する。

### ベースでの基本動作の確認（2026-10-04）

本人がベース/AHT20で実測値のDiscord受信、本体の温湿度表示・現在値/温度/湿度画面・24/12/6時間の期間切替を確認した。さらに給電を切って戻した後もグラフが保持されることを確認し、再度PCへ接続した公開状態でも保存正常・履歴2点を読み取った。通常の電源断後の保存・復元は確認済み。書き込み中の電源断耐性、RTCの電池による時刻保持、欠測区間の表示をすべて実機で検証したとは扱わない。

共有された起動通知は13:46:37 JSTに25.3℃・68.6%、14:02:13 JSTに25.6℃・68.0%で、いずれも測定時刻は「--」、通知時刻は有効だった。実装は時刻修正後に再測定する一方、起動通知はその完了を待たずに送信できる。本人がその後の定期報告で25.6℃・66.7%、測定14:14:17 JST・通知14:14:18 JSTを確認し、通常計測の測定日時と約1分の定期送信を確認した。起動直後の測定時刻が不明な場合の「--」表示は観察として残す。センサーを外したPC接続では、起動/試験の「未取得」と、その2分後の「センサー応答なし」の受信を本人のログで確認した。

短時間の受入のため、元の公開設定をGitの外へ控えてから、計測10秒・定期報告1分・保存1分・高温27℃/継続10秒・再通知1分・回復幅1℃、湿度警報停止へ一時変更し、USBで保存結果を読み返した。ブザーは有効。接続情報は変更せず、コードの書き換えや履歴消去は行っていない。試験は進行中で、最後に元の公開設定へ戻す必要がある。実測値の異常/復帰・ブザー・通信/センサー断時の操作、試験設定の復元と旧Web公開終了はまだ完了としない。
