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
- Codex連携の許可と保存は完了し、移行PR #108をmainへ反映済み。旧open PR 59件をクローズし、保全済み旧ブランチ104本を削除した。GitHub mainとCodexの作業コピーは一致している。
- 整理後に旧open PR 0件、旧ブランチ0本を確認。移行用ブランチを維持し、ローカルの旧リモート参照もpruneした。整理前のbundleと全PR head参照は保持。
- ローカルで作成した移行4コミットはarchive/codex-migration-local-2026-10-03タグへ保持。コネクターで反映したtreeはローカルと完全一致。

## 保全場所

- 継続作業: `C:\Users\pc\Documents\Codex\projects\xiao-scanner`
- 整理前保全: `C:\Users\pc\Documents\Codex\backups\xiao-scanner\2026-10-03`
- Git bundle SHA256: `234b652dad7ac986d78ca178c246a7d0581a0c56314fe537a6e891b2f0ad791f`
- 配布用バックアップZIP SHA256: `81e65f54deefd18fa25b54f1a03bcf29ec5f2bcb2e0006c62cc5ba00f646b79f`

## 続ける工程

JOINT_ACTIONS.mdに従い、Codex登録、旧端末、Jules未公開差分・CI Fixer、実機の共同確認を行う。その後、全受入条件を照合してBへ進む。

## 完了した共同対応

- Codex連携の許可と保存は完了し、移行PR #108をmainへ反映済み。旧open PR 59件をクローズし、保全済み旧ブランチ104本を削除した。GitHub mainとCodexの作業コピーは一致している。
- Julesの温度計へのGitHub書き込みアクセスは解除済み。Scheduled一覧に対象タスクはない。確認待ち12件を一時停止し、レビュー待ち81件と合わせた93件をアーカイブへ移して履歴を保持した。CI Fixerは有効表示だがチェックボックスがdisabledで変更できない。未公開差分の一括エクスポートは未確認で、古いセッションのDownload zipは完了しなかった。

## 残る共同対応

- Codex登録: 作業フォルダは用意済み。プロジェクト一覧には未登録で、現在のツールには追加操作がない。
- Antigravity/Mac: WindowsのAntigravityは起動中。温度計のワークスペース・未保存変更・起動元と、Drive取得後のMac追加データは画面/端末の共同確認が必要。
- Jules: Archivedの未公開作業差分を確認・取得する。CI Fixerの有効表示は操作不能のため残す。権限を戻して新しい自動作業を起動しない。
- 実機: 型番、書き込み済みファームウェアの版、必要なUSB/BLE接続を確認する。


別名のMac移行フォルダは空、Arduinoフォルダにはlibrariesのみ。Driveの追加XIAO ENV Data.txt二件は13バイトの同一タイトル文で、計測ログではなかった。別途保全して分類した。

実際に残った項目をJOINT_ACTIONS.mdにまとめた。共同確認で判明した追加データの取得・比較と最終判定はCodexが続けて担当する。
