# 旧Webスキャナーの運用終了準備

2026-10-04。**終了案内は準備済み、未公開**。作業ブランチはcodex/retire-web-scanner。mainとGitHub Pagesの公開切替は、新しいWi-Fi・Discord版の実機受入後に行う。

## 変更と保全

本人がQR/BLE/Webスキャナーを廃止する判断をしたため、直下index.htmlを日本語の終了案内へ置き換える。温湿度と履歴は本体、通知はDiscordへ移ったことを説明し、[使い方](WIFI_DISCORD.md)へ案内する。カメラ・Bluetooth・外部API・入力欄・JavaScript・外部画像やライブラリは使わない。

旧main 7c42c29e73c67c41340e5f18dc707a768aa9bc23のindex.htmlを、[保管フォルダ](../archives/web-scanner-2026-10-04/README.md)へ同じバイトで保存した。53,095バイト、SHA256はf521a78c66fdbaf4cbc59db74862c78f0114a4c8de60ebc54b765c5de8181588。復元時は保管ファイルを直下index.htmlへコピーする。Git履歴を巻き戻したり、mainを強制更新したりしない。

既存Jestと二つの過去ベンチマークの参照先を保管HTMLへ変更した。テキスト拡張子のHTMLはベンチマークで内容を読み込んで表示し、テキストページとして開かない。テストの判定・ベンチマークの計測処理・依存関係は変更していない。保管資料や旧MCU・学習資料は削除していない。

## 確認結果

- 保管前後のバイト一致とSHA256一致を確認。
- 既存Jest: 2スイート7件成功。標準一時キャッシュの権限エラーがあったため、作業フォルダ内のbuild/jest-cacheを指定して実行した。
- ローカルの案内を実ブラウザで表示。幅390pxと1280pxでレイアウトと横方向にはみ出さないことを確認。1280pxの全体画像でも内容を確認した。
- 「新しい温度計の使い方を見る」を押し、GitHub mainの日本語の使い方ページが開くことを確認。
- 案内にJavaScriptがなく、カメラ/BLE/APIの呼び出しや秘密値の入力がないことを確認。
- 過去の性能測定は今回実行していない。旧Webのカメラ・BLE・API通信の受入ではない。ファームウェアの変更・再書き込みは行っていない。

実行したJestコマンドは、このWindowsの用意済みnpm CLIから `test -- --runInBand --cacheDirectory build/jest-cache`。通常の環境では `npm test -- --runInBand --cacheDirectory build/jest-cache` で同じテストを実行できる。

## 公開までの条件

[共同対応票](WIFI_DISCORD_JOINT_ACTIONS.md)の本体表示・操作、Wi-Fi/NTP、Discord受信、イベントと復帰、履歴保存と再起動、通信断/センサー断の確認が必要。下書きPRを作ったことやWebのテスト通過だけで、実機受入・公開終了を完了にしない。

実機受入後にこの記録・共同対応票・引き継ぎの状態を更新し、案内をmainへ反映する。GitHub Pagesの実際の公開URLで案内を確認してから、運用終了を完了と記録する。現時点では公開切替を実行していない。

[下書きPR #116](https://github.com/kakinymax/xiao-scanner/pull/116)、終了案内の実装SHA `026af4c89ff678befa93095fccf446febf572eec`。mainへの反映・公開切替は未実施。
