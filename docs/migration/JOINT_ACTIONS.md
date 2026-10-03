# 共同対応票

2026-10-03。自力で実施可能な工程を完了し、共同対応中。ユーザーがPRを一つずつ比較する必要はない。GitHubの反映と整理は完了。プロジェクト登録も完了。残る旧端末・実機・未公開差分の共同確認を進める。

## 1. Julesの停止と残る履歴確認

対象: https://jules.google.com/u/1/repo/github/kakinymax/xiao-scanner/overview

完了: GitHubのGoogle Labs Jules設定から温度計を除外し、他の既存2件だけを残した状態を確認した。温度計への書き込みアクセスは解除済み。Scheduled一覧は「No scheduled tasks yet」。確認待ち12件はPausedにし、レビュー待ち81件と合わせて93件をArchivedへ整理した。履歴の永久削除はしていない。

未確認: Julesにだけ残る未公開作業差分の完全な取得。詳細を確認した旧セッションではRead-only・1か月以上経過のロック表示があり、CodexからDownload zipを押したがダウンロード完了を確認できなかった。GitHubに公開済みの全PR・ブランチは保全済みだが、この事実で未公開内容も取得済みとはしない。

共同操作: 上記URLのArchivedから該当セッションを開き、変更ファイルとDownload zipを確認する。取得できたZIP/差分をCodexの保全フォルダへ置き、Codexが既存107 PRの差分と比較する。独自変更は保管資料と後続課題へ結び付け、未検証の変更をmainへ丸ごと取り込まない。アーカイブ済み履歴を削除しない。

CI Fixer確認先: https://jules.google.com/u/1/repo/github/kakinymax/xiao-scanner/ci-fixer

保存済み表示は有効だがチェックボックスがdisabledで操作できず、CI apps一覧は「No CI apps detected yet」。GitHubの書き込み権限は解除済みだが、設定値の無効化は未実施として残す。共同操作で無効化できるか確認し、権限を戻す必要がある場合は新しい作業が再開するため先にCodexと判断する。

## 2. GitHub許可・反映・整理（完了）

ChatGPT Codex Connector（installation 167109775）へ、ユーザーが保存直前に承認した内容でxiao-scannerを追加・保存した。成功画面と許可対象一覧、ブランチ作成と実際の書き込みを確認した。

移行PR [#108](https://github.com/kakinymax/xiao-scanner/pull/108)をmainへ統合。反映コミットはb95919022d6acceef45c79022db6fc05fd1c6fe1。旧open PR 59件は全件クローズし、旧ブランチ104本はSHAと保全を照合して削除した。整理直後のopen PRは0件、ブランチはmainとcodex/migrate-developmentのみ。後者は共同確認完了まで保持する。

GitHub mainとCodexの作業コピーは一致している。追加変更の反映は作業ブランチとPRを使う。認証済みコネクターで反映できるため、今回の移行に認証トークンの転記や端末Gitのpush認証は必要ない。端末Gitのpushを後日使う場合だけGit Credential Managerで設定する。

## 3. Codexのプロジェクト登録（完了）

対象フォルダ: `C:\Users\pc\Documents\Codex\projects\xiao-scanner`

ユーザーが既存フォルダを選んでローカルプロジェクトxiao-scannerを作成した。Codexのプロジェクト一覧でこの絶対パスとGitリポジトリの一致を確認済み。

プロジェクト内にチャットがない状態で登録されている。このチャットでは上記フォルダを指定して編集・テスト・Git同期を継続できる。現在のツールには既存チャットをプロジェクト内へ移す操作がないため、チャット所属の自動変更は行っていない。作業を分散する新しいチャットは作成していない。

## 4. 旧AntigravityとMacに残る最新データ

旧資料にあるフォルダ: `/Users/ejentoyou/mac移行データ/antigravity/scratch/2_xiao_env_scanner_system`

Driveの二つの作業コピー49ファイル、Git参照、未反映差分をCodexへ保全済み。現在のMacには接続できていないため、Driveへ移した後の追加変更・ログ・必要設定は未確認。

現在のWindowsでもAntigravityの起動を確認したが、scratchとbrainの対象名検索では温度計の作業コピーや関連Markdownが見つからなかった。現在のネイティブ画面は操作できないため、ユーザーと開いているワークスペースが温度計を含むか確認する。含む場合は未保存変更・作業コピー・タスクを保全/停止する。他のプロジェクトを一括で閉じたり停止したりしない。

ユーザーとMacの現在の作業フォルダでGit status、独自ファイル、更新日時とログを確認する。追加分があれば先に取得・比較・保全する。旧フォルダの削除は必須ではない。APIキー等の認証情報は公開Gitへ移さず、必要な利用手順だけを記録する。

## 5. 実機の版と接続

コードが対象とする構成: XIAO ESP32C3、Expansion Base、Grove AHT20、OLED SSD1306、RTC PCF8563。スケッチはmonitorとai_collectorの二つで、実機へ書き込まれた版は未確認。

ユーザーは機器を接続し、現在の書き込み記録/使用ファイル/表示を確認する。保存されたログを保全したうえで、USBまたはBLEの必要な接続を検証する。既存版を特定できず書き込みが必要になった場合は、ログ保全と対象版を確定してからビルド済み版を扱う。

Codexと確認すること: 実機の型番とソースの対応、書き込み版の識別、必要な接続が成功したこと。温度異常や転送完全性は別の既知課題で、接続成功だけで修正済みとはしない。

## 完全移行への最終確認

以上の必須項目が終わり、GitHub mainとCodexの作業ソースが一致し、旧自動開発が停止し、文書化した手順で編集・テスト・ビルド・Git同期を再開できることを確認してから完全移行完了にする。
