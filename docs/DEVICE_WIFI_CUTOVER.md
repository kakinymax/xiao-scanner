# 実機をWi-Fi・Discord版へ切り替えた記録

2026-10-04。対象は以前に完成版として保全した同じXIAO ESP32C3。**書き込み・領域の照合・RESET後のUSB起動確認は完了。ベース/センサーと使用環境での受入は確認中。** [設計判断D011](DECISIONS.md)、[検証記録](WIFI_DISCORD_VALIDATION.md)、[共同対応票](WIFI_DISCORD_JOINT_ACTIONS.md)を参照。

記録は[PR #115](https://github.com/kakinymax/xiao-scanner/pull/115)、記録初版SHAはf586fe903e756174e96c195fc64bd80cfc968a31。製品コードと記録の更新を区別する。GitHubから8ファイルを取得し直し、PCの記録との一致を確認した。

## 書き込み前の保全

- USB接続: COM4、ESP32-C3 rev0.4、4MB。旧保全と同じ本体を確認した。機器の個別識別情報は公開文書へ転記しない。
- 新しい保存先: `C:\Users\pc\Documents\Codex\backups\xiao-scanner\wifi-cutover-2026-10-04-01`。旧device-baseline-2026-10-04もそのまま保持。
- フラッシュ原本: flash-before-wifi.bin、4,194,304バイト。SHA256 `6f17c397c400411c5104e169723048a4ea5b7af127b67a69358909779b8be86c`。esptool 5.0.0のverify-flashで実機と全領域のダイジェスト一致を確認した。
- 旧app0は以前の実機プログラムと一致。元の正確なArduinoソースが判明したという意味ではない。
- LittleFSログ: 312,480バイト、26,040件。前回25,440件の内容は変わらず、600件が増えた。SHA256 `e55ded44dc465191c545b31c1bb62da7ba801d461c6977363ee4b0b75c1a7ad8`。
- 以前と同じmklittlefs 4.0.2でコピーから抽出。旧3.0.0が読めないことをファイル破損と扱わない。原本・抽出結果・ハッシュ・読み取り/照合ログはGitの外へ保存した。
- RAM内の未保存分はフラッシュの保全には含まれない。

## 領域の確認と書き込み

切替直前の領域表のMD5を検査し、新版の六つの領域の名前・種別・位置・容量と一致することを確認した。コードと生成物のハッシュも再照合した。

| 内容 | アドレス | 実施 |
|---|---|---|
| 起動プログラム | 0x0000 | 書き込み後のハッシュ一致 |
| 領域表 | 0x8000 | 書き込み後のハッシュ一致 |
| OTA起動情報 | 0xe000 | 標準boot_app0を書き込み、ハッシュ一致 |
| Wi-Fi版のapp0 | 0x10000 | 1,244,448バイトを書き込み、ハッシュ一致 |
| NVS | 0x9000〜0xdfff | 全消去・初期化を行わない |
| 旧LittleFS | 0x290000〜0x3effff | 書き込み対象にしない |

Arduino CLI 1.5.1、ESP32 core 3.3.0、XIAO_ESP32C3の既定設定と--verifyで、コンパイル済みbuild/wifi-discordをアップロードした。全フラッシュ消去の指定は使っていない。書き込みログは保存先のupload-wifi.log。

実装は[PR #114](https://github.com/kakinymax/xiao-scanner/pull/114)のSHA `446c82189b61fbff485d45b97b1f8acb968eb4a9`、版はwifi-discord-1.0.0。共有mainは `669be0b98affa4d0785653ffb3a23197ae4812ab`。今回、製品ソースはまだ変更していない。

## 起動と受入

初回USBのstatusは応答時間切れ。ユーザーがBOOTを押さないRESET操作を行った後、wifi-discord-1.0.0のUSB応答を確認した。保存状態・通信タスクは正常、接続設定は未設定。この時点ではXIAOをベースから外していたため、温湿度未取得・時刻未取得・履歴0点はベース/センサー付きで再確認する。秘密値はまだ入力していない。USBを外してベースとAHT20を戻し、再接続するよう依頼した。

温湿度の取得、画面と期間切替、ブザー、保存と再起動後の復元、Wi-Fi/NTP・実Discord受信、通信断・センサー断からの復帰は[共同対応票](WIFI_DISCORD_JOINT_ACTIONS.md)に残す。旧Webの公開終了は新版の受入後に実施する。

## 接続順の修正とUSBの追加確認

本人がベースへ戻して画面表示を確認した。PCとベースは同時接続できないため、XIAO単体をPCへ接続したままWi-Fi・Webhookなど全設定を終え、最後にベース/AHT20へ戻して受入を行う。具体的な表示値・操作と実通知は未確認のまま区別する。

RESET直後のUSB応答確認後、再確認で5秒/15秒の応答時間切れがあった。一度COM4がなくなり、その後の再接続時はリセット要求なしのROM読取でESP32-C3 rev0.4・4MBの待機応答を確認した。標準のRTSリセットでは新版の応答がなかったが、メーカーの説明に従ってwatchdog-resetで全体をリセットすると、PC接続を保ったままwifi-discord-1.0.0の応答へ戻った。フラッシュの書き込み・消去・設定変更は行っていない。USBのRTSだけでは手動で入ったROM待機状態から戻らない場合があるとの[メーカー資料](https://docs.espressif.com/projects/esptool/en/latest/esp32c3/troubleshooting.html#leaving-download-mode-in-usb-serial-jtag-mode)に沿う観察。PCとベースの同時接続を前提にした確認は取り下げる。

追加の観察記録はGitの外の保存先のlatest-runtime-probe.json、usb-rom-probe.log、setup-order-probe.json、usb-rom-probe-reconnected.log、usb-normal-boot-reset.log、runtime-after-software-reset.json、usb-watchdog-reset.log、runtime-after-watchdog-reset.json。機器の個別識別情報・秘密値を公開文書へ転記しない。

## USB通信修正版1.0.1

通常起動への復帰後、USBの長い状態要求に応答がなく、別の短い要求でも状態応答がJSONとして不完全な場合があった。使用中のHWCDCは未指定時の送受信容量が各256バイトで、最大1535バイトの設定要求と長い状態応答に容量が合わない。判断D013を先に記録し、各2048バイトへ設定したwifi-discord-1.0.1をコンパイルした。

- プログラム1,244,326 / 1,310,720バイト、静的RAM43,992 / 327,680バイト。送受信キューの動的確保は合計3584バイト増えるほか管理領域がある。静的RAM表示は実行中の最大メモリではない。
- 領域表は以前のビルドと一致。接続情報は未入力・履歴0点で、以前の旧4MBと26,040件の保全原本を保持した。
- app0の0x10000だけへ1,244,480バイトを書き込み、ハッシュ一致を確認。起動プログラム・領域表・NVS・旧LittleFSを書き込み対象にせず、消去範囲は0x10000〜0x13ffff。終了時のwatchdog-resetで通常起動へ戻した。
- 書き込みバイナリのSHA256はc2312bbabe4434f9cef6c4e577ac4acc5b3bb1d0acdf8ed5c9edba0370511c13。
- 状態要求だけで実機21項目に成功。64/256/512/1024/1535バイトを各3回、1535バイトの日本語入力、1536バイトの拒否と次の状態応答、再オープン3回。設定が変わらないことも確認した。
- C++733件・PC設定ツール7件も再確認。観察・ビルド・書き込み・通信結果はGitの外のusb-fix-source-manifest.json、usb-fix-build-manifest.json、upload-usb-fix.log、upload-usb-fix-result.json、usb-transport-checks-after.jsonに保持。

本人がDiscord Webhookの準備完了を確認し、XIAO単体をPC接続したまま非表示入力の対話ツールを開いた。実設定・受信と最終のベース受入はまだ別に確認する。旧Web公開は変更していない。

## 旧版に戻すための保全

同じ本体の切替直前の4MB原本を、サイズとSHA256・実機識別を照合した上で0x0000へ戻す方法を保持する。これには当時の起動プログラム、領域表、プログラム、NVS、LittleFSが含まれる。戻した後は原本と実機のダイジェストを照合し、通常画面を確認する。復元書き込み自体はまだ実施していない。復元後は新版で追加した設定・履歴を同時に維持した状態にはならないため、切替後のデータも別途保全してから扱う。
