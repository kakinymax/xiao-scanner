# Webスキャナーの配置

現行Webアプリはリポジトリ直下の **index.html**。GitHub Pagesとローカル確認も同じファイルを使う。このフォルダのarchive/は過去版の保管場所。

機能はQRデコード、BLE同期、温湿度・不快指数のグラフ、CSV保存、Gemini APIによるアドバイス。実際の接続・通信の検証状況はdocs/migration/STATUS.mdとISSUES.mdで確認する。

開発はCodexで行い、README.md、HANDOVER.md、docs/DEVELOPMENT.mdの手順を使う。Driveのweb_scanner/index.html作業版はarchives/antigravity-2026-05-21へ保存してあり、現行版とは区別する。

現行APIキー保存先はブラウザlocalStorage。キーをリポジトリやログへ記録しない。保存方法と画面の説明の見直しはISSUES.mdに残る。
