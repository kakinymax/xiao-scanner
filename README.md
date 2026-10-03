# xiao-scanner — XIAO温度計

保存中の旧版は、XIAO ESP32C3で温度・湿度・不快指数を記録し、QR/BLEでブラウザへ渡す環境モニター。開発窓口はCodex、共有するソースと履歴の基準はGitHub main。

2026-10-04に、常時給電で温湿度・履歴・ブザーを残し、Wi-FiからDiscordへ通知する新版を実装。[新しい温度計の使い方](docs/WIFI_DISCORD.md)、[検証記録](docs/WIFI_DISCORD_VALIDATION.md)、[実機切替の共同対応票](docs/WIFI_DISCORD_JOINT_ACTIONS.md)を参照する。Webスキャナー・QR/BLE・不快指数・AI予測/学習ログは新スケッチに含めない。実機切替・実通知・旧Web公開の終了は未確認の別工程。

[プロジェクトのヒストリー](docs/PROJECT_HISTORY.md)で構想・変更・失敗とコードの対応を追える。[設計判断](docs/DECISIONS.md)、[説明用メモ](docs/PORTFOLIO.md)、[履歴の根拠](docs/history/antigravity-history-evidence.md)も参照する。

## 開発を再開する

1. Codexでこのリポジトリを開く。初期移行先は `C:\Users\pc\Documents\Codex\projects\xiao-scanner`。
2. [HANDOVER.md](HANDOVER.md)、[ISSUES.md](ISSUES.md)、[移行状況](docs/migration/STATUS.md)を読む。
3. [開発手順](docs/DEVELOPMENT.md)に従って依存関係とテストを確認し、作業ブランチで修正する。

移行PR [#108](https://github.com/kakinymax/xiao-scanner/pull/108)をmainへ反映し、Codexの作業コピーとの一致を確認した。旧PR59件と旧ブランチ104本は、履歴を保全して整理済み。

**2026-10-04にCodexへの移行を完了。** Julesの対象GitHub連携と書き込みアクセスを解除し、確認待ち12件を停止、旧セッション93件をアーカイブへ整理した。Codexへ既存フォルダをプロジェクト登録済み。ユーザー指示により、Jules未公開作業の全件回収、Macの追加調査、現在の実機確認は移行の必須条件から外した。[共同対応の完了記録](docs/migration/JOINT_ACTIONS.md)を参照。

## 編集対象

| 場所 | 役割 |
|---|---|
| mcu_firmware/xiao_env_wifi_discord/ | 新しい開発対象。常時給電・温湿度・履歴・ブザー・Wi-Fi/Discord通知 |
| tools/setup_temperature.py | USBで本体の接続情報・通知条件・画面などを設定するPCツール |
| index.html | 現行Web画面。GitHub Pagesも直下のこのファイルを使う |
| mcu_firmware/xiao_env_monitor/ | 通常の温湿度モニター用スケッチ |
| mcu_firmware/xiao_env_ai_collector/ | LittleFSにAIログを蓄積し、TinyMLモデルを使うスケッチ |
| ai_training/ | 既存ログ、旧形式パーサー、学習ガイド |
| tests/ | 旧WebのJestテスト、過去のベンチマーク、新版のC++/PC設定ツールの検証 |
| archives/ | 旧ファームウェアと未完成作業版の保管 |
| docs/history/ | 移行前の説明・引き継ぎ・Julesの判断記録 |
| docs/migration/ | 移行状況、全PR・ブランチの整理台帳 |
| AGENTS.md | Codex向け作業指示 |

次に実機へ切り替える対象はwifi_discord。旧monitor/ai_collectorを新しいWi-Fi版として書き込まない。現在の実機はAI収集版の系統を読み取り保全済みだが、正確な元ソースは未確認。[実機基準](docs/DEVICE_BASELINE.md)を読む。

## 既知課題と旧作業版

温度が60〜70℃として表示される症状、ログの完全性、転送と時系列の扱いは未解決。[ISSUES.md](ISSUES.md)を参照。Driveの16バイトヘッダー対応作業版は対応ファームウェアがないため、[保管資料](archives/antigravity-2026-05-21/README.md)として引き継ぐ。

現行Web画面のGemini APIキーはブラウザlocalStorageへ保存する実装。キーはGitへ記録しない。カメラ・BLE・API通信の実動作は接続・権限を確認して検証する。

## 移行で実施した変更

- 全107 PR・105ブランチの整理前履歴を保全し、復元を検証。
- Driveの二つの作業コピーと未反映差分を保全。
- AI応答を表示するsetSafeHTMLの未定義を、PR #104由来の最小実装で補い、文字列をHTMLとして実行しない回帰確認を追加。
- Codex向けの作業指示と開発手順を整備し、移行PR #108をmainへ反映。
- 旧open PR 59件、旧作業ブランチ104本を保全後に整理。
- Julesの書き込みアクセス解除、旧確認待ちタスクの停止とアーカイブ。

[移行前の更新履歴](docs/history/README-before-migration.md)も保持している。
