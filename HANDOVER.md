# Codex引き継ぎ

更新日: 2026-10-03。A: Codex実施分完了・共同対応待ち。完全移行は未完了。

## 作業の基準

- 対象: kakinymax/xiao-scanner。
- 共有ソース: GitHub main。整理前の基準SHAは53cf755aad7ae8b80544a19d68c584b52b8533c7。
- 継続作業フォルダ: `C:\Users\pc\Documents\Codex\projects\xiao-scanner`。
- 移行PR #108反映後のmain: b95919022d6acceef45c79022db6fc05fd1c6fe1。
- 移行作業ブランチ: codex/migrate-development（共同確認中のため保持）。
- 整理結果の追記は専用作業ブランチからPRで反映する。
- 方針: 開発窓口をCodexへ一本化する。独自変更の保全後にGitHubと旧運用を整理する。ユーザー固有の操作は、自力で実施可能な工程を終えてからまとめて共同対応する。

## 現在の証拠と次の作業

[移行状況](docs/migration/STATUS.md)、[全PR台帳](docs/migration/PR_LEDGER.md)、[全ブランチ台帳](docs/migration/BRANCH_LEDGER.md)、[共同対応票](docs/migration/JOINT_ACTIONS.md)を読む。現在のGit状態と台帳を照合し、共同対応から続ける。テスト7件成功、ESP32 core 3.3.0で現行スケッチ二つのビルド成功、ブラウザ基本動作確認済み。

Codex連携の許可と保存は完了し、移行PR #108をmainへ反映済み。旧open PR 59件をクローズし、保全済み旧ブランチ104本を削除した。GitHub mainとCodexの作業コピーは一致している。

Julesの温度計へのGitHub書き込みアクセスは解除済み。Scheduled一覧に対象タスクはない。確認待ち12件を一時停止し、レビュー待ち81件と合わせた93件をアーカイブへ移して履歴を保持した。CI Fixerは有効表示だがチェックボックスがdisabledで変更できない。未公開差分の一括エクスポートは未確認で、古いセッションのDownload zipは完了しなかった。

次の共同対応は、Codexへの既存フォルダ登録、旧Antigravity/Macの未反映データと起動元、実機の版と接続、Julesにだけ残る未公開差分の取得確認。CI Fixerの状態も共同対応票へ残す。端末Gitのpush認証はコネクター経由の反映には不要。

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
