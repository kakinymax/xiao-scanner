# 共同対応票

2026-10-03。自力で実施可能な工程を完了し、共同対応中。ユーザーがPRを一つずつ比較する必要はない。認証・接続後のコード反映とPR/ブランチ整理は引き続きCodexが担当する。

## 1. 以前のJules実行元を停止する

対象: https://jules.google.com/ と https://github.com/settings/installations

ユーザーのサインイン後、アプリ内ブラウザで温度計の履歴を持つJulesアカウントとGitHubのkakinymaxアカウントを確認した。Julesでxiao-scannerを選択したScheduled一覧は「No scheduled tasks yet」。全セッションの停止確認はまだ終わっていない。

Google Labs JulesのGitHub App（installation 133726235）はAll repositoriesだった。温度計を除外して他の既存2件を残す選択内容を準備したが、Saveと状態取得が自動承認レビューに拒否された。その後ユーザーが保存し、CodexがGitHub設定を読み直してOnly select repositoriesと他2件だけが残る状態を確認した。xiao-scannerへのJulesの書き込みアクセスは解除済み。この方式では今後作るリポジトリも自動追加されない。

ユーザーは以前利用したGoogle/GitHubアカウントでサインインする。対象をkakinymax/xiao-scannerに絞り、実行中タスク・継続/定期タスクを停止し、JulesのGitHub連携からこのリポジトリを外す。Antigravityやローカル側でこのプロジェクトを起動する定期処理がある場合も停止する。他のプロジェクトの設定は変更しない。

Codexと確認すること: 対象の実行中/定期タスクがないこと、対象リポジトリへの連携が外れたこと。旧アカウントにアクセスできない場合はGitHub側の対象アクセス解除も確認する。.jules資料の移動だけでは停止済みにならない。

## 2. GitHubの温度計リポジトリへの許可と認証

対象: https://github.com/settings/installations/167109775

当初は温度計が許可対象になく、ブランチ作成とPRクローズが403で拒否された。共同対応で、ユーザーが保存直前に承認した内容に従ってCodexがChatGPT Codex Connector（installation 167109775）へxiao-scannerを追加・保存。成功メッセージ、コネクターの許可対象一覧、移行ブランチcodex/migrate-developmentの作成成功を確認した。コード反映と旧PR・ブランチ整理を続ける。

今回のコード反映は認証済みコネクターを使う。端末Gitでpushする場合には別途認証が必要だが、通常のGitHub閲覧用ログインやコネクターの許可とは別である。ブランチ整理は認証済みGitHub画面で可能な操作から進める。認証トークンをチャットや文書に貼らない。

後日、端末Gitのpushも使う場合はGit Credential Managerで認証する。今回のコネクター反映に追加トークンを転記する必要はない。

```powershell
$env:GIT_EXEC_PATH = 'C:\Users\pc\.cache\codex-runtimes\codex-primary-runtime\dependencies\native\git\mingw64\bin'
git credential-manager github login
```

許可・認証後はCodexが最新mainと差分を照合し、移行ブランチを反映・レビュー可能にまとめてmainへ統合する。その後、全PR台帳に従って59件の旧open PRを整理し、全ブランチ台帳の104本を、保全とSHAの再確認後に整理する。数字は整理前の基準で、新しい変更があれば再照合する。

## 3. Codexのプロジェクト登録

対象フォルダ: `C:\Users\pc\Documents\Codex\projects\xiao-scanner`

チェックアウトと作業指示は用意済み。登録済みプロジェクト一覧に温度計はなく、現在のツールにはプロジェクト登録操作がない。

ユーザーはCodexのプロジェクト追加からこの既存フォルダを登録する。このチャットを継続し、対象フォルダで編集・テスト・Git同期を行う。登録後、Codexでプロジェクト一覧と作業パスを確認する。新しいチャットへの作業分散は不要。

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
