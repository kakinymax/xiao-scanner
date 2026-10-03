# Codex作業指示

この温度計プロジェクトの開発窓口はCodex。GitHub mainを共有ソース、Codexのチェックアウトを編集場所として扱う。

## 開始時

- HANDOVER.md、ISSUES.md、docs/migration/STATUS.md、docs/DEVELOPMENT.mdを読む。
- Gitの現在ブランチ、未コミット変更、origin/mainとの差分を確認する。既存のユーザー変更を上書きしない。
- 一課題につき一作業ブランチを使い、変更理由と確認結果を記録する。
- 製品コードを変更する前にdocs/PROJECT_HISTORY.md、docs/DECISIONS.md、docs/REDESIGN.mdを確認する。目的・変更理由・機能の追加/廃止判断をDECISIONS.mdへ先に記録し、実装後に検証結果・PR・SHAを追記する。説明に使える経緯はPROJECT_HISTORY.mdとdocs/PORTFOLIO.mdへ反映する。

## 検証と反映

- Web変更は既存のJestテストを実行し、変更した画面の基本動作を確認する。
- ファームウェア変更は対象のXIAO ESP32C3用スケッチをコンパイルする。USB書き込みやログ消去は実機の状態とデータ保全を確認してから扱う。
- 検証後にGitHubへ反映し、反映先のSHAと内容を読み直す。接続済みGitHubコネクターによる反映も利用できる。
- APIキー、認証トークン、ブラウザの私的情報をGitや引き継ぎ文書へ保存しない。

## 整理と引き継ぎ

- 新しい自動化フレームワーク、定期実行、Julesタスクを導入しない。既存テストと標準ツールを使う。
- archives/とdocs/history/は保管資料。通常の編集対象は直下のindex.html、mcu_firmware/の現行スケッチ、ai_training/。
- 独自変更や履歴を失うPR・ブランチ整理は行わない。保全と復元方法を先に確認する。mainの履歴を書き換えない。
- ISSUES.mdは未解決課題が残る間維持する。原因の仮説を検証済みの事実として記載しない。
- ユーザー固有の権限や実機操作が必要な項目は共同対応票にまとめ、他の実施可能な作業を続ける。
- 完了・未確認・共同対応待ちを区別してHANDOVER.mdと移行状況を更新する。
