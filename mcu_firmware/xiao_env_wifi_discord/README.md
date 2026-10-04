# Wi-Fi・Discord版（新しい開発対象）

現在版wifi-discord-1.1.0は0時の温湿度PNGグラフと、別Webhookへの毎時の温度悪化/復帰通知。[仕様・設定・検証](../../docs/DAILY_TREND.md)を参照。下記の定期報告/湿度通知に関する1.0.xの説明は旧仕様。

対象: XIAO ESP32C3 + XIAO拡張ボード + Grove AHT20。常時給電で温湿度・履歴・ブザーを動かし、Discordへ定期・状態変化を通知する。接続情報はUSBで本体へ保存し、ソースへ埋め込まない。

- [日本語の使い方・初期設定](../../docs/WIFI_DISCORD.md)
- [PC・本体用ビルドの検証記録](../../docs/WIFI_DISCORD_VALIDATION.md)
- [実機切替と確認の共同対応票](../../docs/WIFI_DISCORD_JOINT_ACTIONS.md)
- [選んだ理由と過去の経過](../../docs/PROJECT_HISTORY.md)

`thermo_core.h`は計測値の変換・通知判定・履歴形式をまとめた処理で、PCテストにもそのまま使う。`app.cpp`は計測・RTC・画面・ボタン・設定・保存を扱う。`network_worker.cpp`は証明書を検証するHTTPS送信を別タスクで扱う。旧QR/BLE/AIの実装を含まない。

Arduino CLIで`esp32:esp32:XIAO_ESP32C3`を対象にコンパイルする。USB CDCはEnabled、パーティションはDefault 4MB。U8g2 2.35.30とRTClib 2.1.4を使う。WiFi、Preferences、cJSON、HTTPS、証明書束はESP32 core 3.3.0付属のものを使う。新しいセンサー処理はI2Cを直接使い、応答待ちに上限を設ける。

コンパイル済みでも実機確認が済んだとはしない。書き込み前に追加ログと原本の保全、実機・パーティション構成の確認を行う。
