# Codex引き継ぎ

更新日: 2026-10-03。A: Codex実施分完了・共同対応待ち。完全移行は未完了。

## 作業の基準

- 対象: kakinymax/xiao-scanner。
- 共有ソース: GitHub main。整理前の基準SHAは53cf755aad7ae8b80544a19d68c584b52b8533c7。
- 継続作業フォルダ: `C:\Users\pc\Documents\Codex\projects\xiao-scanner`。
- 移行作業ブランチ: codex/migrate-development。
- 方針: 開発窓口をCodexへ一本化する。独自変更の保全後にGitHubと旧運用を整理する。ユーザー固有の操作は、自力で実施可能な工程を終えてからまとめて共同対応する。

## 現在の証拠と次の作業

[移行状況](docs/migration/STATUS.md)、[全PR台帳](docs/migration/PR_LEDGER.md)、[全ブランチ台帳](docs/migration/BRANCH_LEDGER.md)、[共同対応票](docs/migration/JOINT_ACTIONS.md)を読む。現在のGit状態と台帳を照合し、共同対応から続ける。テスト7件成功、ESP32 core 3.3.0で現行スケッチ二つのビルド成功、ブラウザ基本動作確認済み。

共同対応でJulesのGitHub設定を保存し、温度計への書き込みアクセスが解除されたことをCodexが読み直して確認した。対象Scheduled一覧は「なし」。古い確認待ちセッションはRead-only・1か月経過のロック表示で、履歴を残して整理を続ける。ユーザーが保存直前に承認した内容でCodex連携へ温度計を追加し、コネクターの許可対象と移行ブランチ作成成功を確認した。コネクターでコード反映と旧PR・ブランチ整理を進める。端末Gitのpush認証を使う場合は別途設定する。登録済みCodexプロジェクト一覧には温度計がまだない。Macの最新未反映データと実機の版も最終確認が必要。

## 実装とデータの注意

- 編集するWeb本体は直下のindex.html。古いweb_scanner内の作業版へ上書きしない。
- 現行AIログは温度・湿度・DIを各4バイトで保存する旧12バイト形式。ファームウェアには16バイトヘッダー/AI_HDR対応がない。
- 未完成のヘッダー対応Web/Pythonはarchives/antigravity-2026-05-21へ保存。
- サンプルログ76,160バイトは旧形式の12で割り切れず8バイト余る。原因は未検証。温度異常の原因をパケット欠損と断定しない。
- TinyMLのmodel.hと学習ガイドを保持。移行のために再学習やログ形式全面変更は行わない。
- 旧Jules資料はdocs/history/julesへ移動。移動だけでは外部の自動実行は停止しない。

## 継続するルール

AGENTS.mdと[開発手順](docs/DEVELOPMENT.md)を使う。一課題一ブランチ、変更に応じたテスト・ビルド、GitHub反映後のSHA照合を行う。新しい自動化フレームワークや定期実行は導入しない。ISSUES.mdを未解決のまま削除しない。

移行前の説明と判断履歴はdocs/history/へ保持。完全移行は、実施可能な工程と必須の共同対応の両方が終わってから完了にする。
