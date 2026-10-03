# Codex引き継ぎ

更新日: 2026-10-04。**B: Codexへの完全移行完了**。移行に必要な共同対応は残っていない。

## 作業の基準

- 対象: kakinymax/xiao-scanner。
- 共有ソース: GitHub main。整理前の基準SHAは53cf755aad7ae8b80544a19d68c584b52b8533c7。
- 継続作業フォルダ: `C:\Users\pc\Documents\Codex\projects\xiao-scanner`。
- Codexのローカルプロジェクトxiao-scannerを登録済み。今後はCodexを開発窓口とする。
- 移行PR #108、整理記録PR #109、登録記録PR #110をmainへ反映済み。完了記録の親mainは551dfe6ef30d412799267cd973c4ead804837e5b。最新mainは作業開始時にGitHubと照合する。
- 共同確認のため保持していたcodex/migrate-developmentは、履歴保全を確認して2026-10-04にGitHubから削除した。ローカルの保全参照は残す。

## 移行の完了根拠

GitHubの全107旧PR・105旧ブランチとDriveの二つの作業コピーを保全し、復元を検証した。旧open PR 59件をクローズし、旧作業ブランチ104本を削除。独自差分・未完成版・判断記録は保管資料と台帳へ残した。Codex連携によるGitHub書き込みと、mainへの反映・取得・内容照合を確認済み。

JulesのGitHub連携からxiao-scannerを除外し、このリポジトリへの書き込みアクセスを解除した。Scheduledに対象タスクはなく、確認待ち12件を停止、レビュー待ち81件と合わせ93件をアーカイブへ移した。自動更新の停止と対象連携解除という移行目的は達成済み。他のリポジトリの設定は保持した。

2026-10-04のユーザー指示で、Julesの未公開作業の全件把握・回収を移行の必須条件から外した。履歴はアーカイブに保持し、全件取得済みとは扱わない。CI Fixerは有効表示だがdisabledで操作不能だった。設定値を無効化したとは記録せず、GitHub書き込み権限の解除を停止の根拠とする。

ユーザー確認により、MacではDrive移行後に作業しておらず、追加データ調査は不要。Windows内の検索で別の温度計作業コピーは見つからず、今後の編集場所は上記Codexフォルダへ統一する。Antigravityの他プロジェクトを一括停止・削除する必要はない。

現在の実機の版・USB/BLE接続確認は、今後コードを書き換えるため移行の必須条件から外した。実機への書き込み・ログ消去は今回行っていない。必要な実機検証は後続の開発時に扱う。

## 次の開発

[既知課題](ISSUES.md)から修正対象を選び、[開発手順](docs/DEVELOPMENT.md)に従って一課題一ブランチで進める。Jest 2スイート7件、monitor/ai_collectorのXIAO ESP32C3向けビルド、ブラウザ初期表示・設定開閉は確認済み。移行完了は温度異常やBLE転送を修正済みという意味ではない。

[移行状況](docs/migration/STATUS.md)、[全PR台帳](docs/migration/PR_LEDGER.md)、[全ブランチ台帳](docs/migration/BRANCH_LEDGER.md)、[共同対応の完了記録](docs/migration/JOINT_ACTIONS.md)を参照する。現在のチャットはプロジェクト外だが、上記フォルダを指定して作業を継続できる。

## 実装とデータの注意

- 編集するWeb本体は直下のindex.html。古いweb_scanner内の作業版へ上書きしない。
- 現行AIログは温度・湿度・DIを各4バイトで保存する旧12バイト形式。ファームウェアには16バイトヘッダー/AI_HDR対応がない。
- 未完成のヘッダー対応Web/Pythonはarchives/antigravity-2026-05-21へ保存。
- サンプルログ76,160バイトは旧形式の12で割り切れず8バイト余る。原因は未検証。温度異常の原因をパケット欠損と断定しない。
- TinyMLのmodel.hと学習ガイドを保持。移行のために再学習やログ形式全面変更は行っていない。
- 旧Jules資料はdocs/history/julesへ保持。新しいJulesタスクや定期実行を作らない。

AGENTS.mdを使い、変更に応じたテスト・ビルドとGitHub反映後のSHA照合を行う。未解決のISSUES.mdは維持する。
