# Codex移行状況

2026-10-03。ロードマップは確定済み。**A: Codex実施分完了・共同対応待ち**。B: 完全移行の完了条件は未達。

## 確認済み

- 基準main: 53cf755aad7ae8b80544a19d68c584b52b8533c7。
- 調査時点で全107 PR、105ブランチ、未処理PR 59件。
- 全ブランチと全PR headをGit bundleへ保全。別リポジトリへの復元、git fsck、全107 PR参照を確認。
- Driveの二つの作業コピー49ファイルを原本のバイト列で保存し、SHA256を記録。
- 旧Arduinoライブラリ5件の版情報、旧作業コピーのHEAD/main/packed-refsも保存。packed-refsの23コミットはGit保全データ内に存在する。
- 主作業コピーの未反映Web/Python変更はarchives/antigravity-2026-05-21/へ保存。現行ファームウェアと対応しないヘッダー形式をそのまま実装へ上書きしていない。
- Codexから既存Webテスト5件に成功。PR #104に由来するsetSafeHTML修正と回帰テスト追加後は7件成功。
- Arduino CLI 1.5.1、ESP32 core 3.3.0と旧環境のライブラリ版を導入。長いGCCヘッダー検索パスを短いdataフォルダで解消し、monitor/ai_collectorの両方のコンパイルに成功。実機への書き込みは行っていない。
- ローカルブラウザで初期表示・設定の開閉を確認。CDNのjsQR/Chart.jsのSHA384が現行SRIと一致。
- 全PR・ブランチの判断を台帳へ記録し、GitHubの書き込み許可不足を実操作で確認。反映・クローズ・削除は認証後の共同対応へ残す。

## 保全場所

- 継続作業: `C:\Users\pc\Documents\Codex\projects\xiao-scanner`
- 整理前保全: `C:\Users\pc\Documents\Codex\backups\xiao-scanner\2026-10-03`
- Git bundle SHA256: `234b652dad7ac986d78ca178c246a7d0581a0c56314fe537a6e891b2f0ad791f`
- 配布用バックアップZIP SHA256: `81e65f54deefd18fa25b54f1a03bcf29ec5f2bcb2e0006c62cc5ba00f646b79f`

## 続ける工程

JOINT_ACTIONS.mdに従い、Julesの旧実行元停止、GitHub許可・認証と移行反映/PR・ブランチ整理、Codex登録、Mac・実機確認をユーザーとまとめて実施する。その後、全受入条件を照合してBへ進む。

## 共同対応候補

- Jules: 対象Scheduled一覧は「なし」。ユーザーによる保存後、GitHub設定でOnly select repositoriesに他の既存2件だけが残り、温度計が除外されたことを確認した。Julesの温度計への書き込みアクセスは解除済み。確認待ちセッションの詳細は「1か月以上経過したためロック」・Read-only表示。履歴を保持してセッション整理とCI Fixer設定の確認を続ける。
- GitHub反映/PR整理/ブランチ削除: ユーザーが保存直前に承認した内容で、Codex連携に温度計を追加・保存した。成功画面とコネクターの許可対象一覧を確認し、codex/migrate-developmentの作成も成功した。以前の403は解消。最新mainは整理前の基準SHAと一致。コネクターでコード反映・PR整理を進め、ブランチ整理も認証済み画面を利用する。端末Gitでpushを使う場合の認証は別途必要。
- Codexプロジェクト登録: 作業フォルダは存在するが、登録済みプロジェクト一覧に温度計はない。
- Antigravity/Mac: WindowsでAntigravityが起動中だが、scratch/brainの対象名検索では温度計が見つからなかった。画面での対象ワークスペース確認と、Drive以外のMacの未反映データの最終確認を共同対応へ残す。
- 実機: 接続、書き込み済みファームウェアの版の特定、必要なUSB/BLE確認。

別名のMac移行フォルダは空、Arduinoフォルダにはlibrariesのみ。Driveの追加XIAO ENV Data.txt二件は13バイトの同一タイトル文で、計測ログではなかった。別途保全して分類した。

実際に残った項目をJOINT_ACTIONS.mdにまとめた。認証・接続後の反映と整理もCodexが続けて担当する。
