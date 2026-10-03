# xiao-scanner — XIAO温度計

XIAO ESP32C3で温度・湿度・不快指数を記録し、QR/BLEでブラウザへ渡す環境モニター。今後の開発窓口はCodex、共有するソースと履歴の基準はGitHub main。

## 開発を再開する

1. Codexでこのリポジトリを開く。初期移行先は `C:\Users\pc\Documents\Codex\projects\xiao-scanner`。
2. [HANDOVER.md](HANDOVER.md)、[ISSUES.md](ISSUES.md)、[移行状況](docs/migration/STATUS.md)を読む。
3. [開発手順](docs/DEVELOPMENT.md)に従って依存関係とテストを確認し、作業ブランチで修正する。

共同対応でJulesの温度計への書き込みアクセスを解除し、Codex連携への追加・保存と移行ブランチ作成を確認した。コード反映と旧PR・ブランチ整理を進めている。旧セッションの整理、Codex登録、Mac・実機確認が残る。完全移行済みとは扱わない。[共同対応票](docs/migration/JOINT_ACTIONS.md)の手順で進める。

## 編集対象

| 場所 | 役割 |
|---|---|
| index.html | 現行Web画面。GitHub Pagesも直下のこのファイルを使う |
| mcu_firmware/xiao_env_monitor/ | 通常の温湿度モニター用スケッチ |
| mcu_firmware/xiao_env_ai_collector/ | LittleFSにAIログを蓄積し、TinyMLモデルを使うスケッチ |
| ai_training/ | 既存ログ、旧形式パーサー、学習ガイド |
| tests/ | Jestの機能テストと過去のベンチマーク |
| archives/ | 旧ファームウェアと未完成作業版の保管 |
| docs/history/ | 移行前の説明・引き継ぎ・Julesの判断記録 |
| docs/migration/ | 移行状況、全PR・ブランチの整理台帳 |
| AGENTS.md | Codex向け作業指示 |

通常はmonitorかai_collectorのどちらか一方を実機に書き込む。現在実機で動いている版は未確認。

## 既知課題と旧作業版

温度が60〜70℃として表示される症状、ログの完全性、転送と時系列の扱いは未解決。[ISSUES.md](ISSUES.md)を参照。Driveの16バイトヘッダー対応作業版は対応ファームウェアがないため、[保管資料](archives/antigravity-2026-05-21/README.md)として引き継ぐ。

現行Web画面のGemini APIキーはブラウザlocalStorageへ保存する実装。キーはGitへ記録しない。カメラ・BLE・API通信の実動作は接続・権限を確認して検証する。

## 移行で実施した変更

- 全107 PR・105ブランチの整理前履歴を保全し、復元を検証。
- Driveの二つの作業コピーと未反映差分を保全。
- AI応答を表示するsetSafeHTMLの未定義を、PR #104由来の最小実装で補い、文字列をHTMLとして実行しない回帰確認を追加。
- Codex向けの作業指示と開発手順を整備。

[移行前の更新履歴](docs/history/README-before-migration.md)も保持している。
