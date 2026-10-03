# 移行検証記録

2026-10-03。ソース変更と環境準備をローカルで検証した記録。GitHub反映と実機確認は未実施。

| 対象 | 検証結果 | 範囲 |
|---|---|---|
| Git履歴 | git fsck、bundle verify、別リポジトリへの復元、107 PR参照を確認 | 整理前の全PR/ブランチ |
| Drive原本 | 49ソースファイル+5ライブラリ版情報+5 Git参照のサイズ/SHA256を確認 | 59ファイル。配布ZIP内のバイト列も一致 |
| Web | Jest 2スイート、7件成功 | initBlocksの既存5件、AI表示の回帰2件 |
| CDN | jsQR/Chart.jsの実ファイルのSHA384がSRIと一致 | 現行index.htmlの二つのCDN資源 |
| ブラウザ | 初期画面と設定の開閉を確認、観測したエラーログなし | カメラ・BLE・API実通信は対象外 |
| monitor | ESP32 core 3.3.0でコンパイル成功 | XIAO_ESP32C3、714,538 bytes (54%)、RAM 24,944 bytes |
| ai_collector | ESP32 core 3.3.0でコンパイル成功 | XIAO_ESP32C3、786,058 bytes (59%)、RAM 25,112 bytes |
| GitHub書き込み | ブランチ作成とPRクローズはいずれも403 | 現在の連携に対象リポジトリが含まれない。リモート変更なし |
| 端末Git push | 非対話dry-runが認証不足で失敗 | 認証は共同対応待ち |

## ビルド環境と成果物

Arduino CLI 1.5.1、ESP32 core 3.3.0、FQBN esp32:esp32:XIAO_ESP32C3。ライブラリ版と再開コマンドはdocs/DEVELOPMENT.mdに記録。

- monitorのアプリBIN SHA256: `75282f963eeff6adacb662bfcca0e88a7945268f8a32e54817d7817cdf3f625d`
- ai_collectorのアプリBIN SHA256: `fc1dfa5aeb8abaf826137bce0f353a18dc20d589702b38b82f0043ed22336734`
- ビルド生成物はbuild/配下でGit管理対象外。書き込み済み実機の識別にはまだ使っていない。

最初は長いArduino dataのパスで、GCCのSDK検索フラグを使う場合だけbits/error_constants.h探索が失敗した。dataをC:\Users\pc\Documents\Codex\mcuへ移し、ソースを変えずに両スケッチのコンパイルが成功した。Codexのsandboxユーザーでは一部のパッケージ探索がアクセス拒否になるため、コンパイルと導入版確認は許可された通常ユーザー実行で検証している。

既存の小さいバッファへのsprintfと未使用コード等の警告が残る。ISSUES.mdの後続課題として保持し、警告なし・温度異常修正済み・転送完全性確認済みとは扱わない。
