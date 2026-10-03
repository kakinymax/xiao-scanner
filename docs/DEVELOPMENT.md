# Codexでの開発手順

初期移行先はWindowsの `C:\Users\pc\Documents\Codex\projects\xiao-scanner`。別端末へ移す場合はGitHubから取得し、依存関係を以下の版で用意する。

## WebとPython

確認済みの実行環境はNode.js 24.19.0、npm 12.2.0、Python 3.12.14。Webテストの依存関係はpackage-lock.jsonに固定する。

通常の環境ではリポジトリ直下で `npm ci`、`npm test -- --runInBand` を実行する。この移行先ではnpmがPATHにないため、以下の入口を使える。

```powershell
$npmCli = 'C:\Users\pc\Documents\Codex\tools\temperature\npm\node_modules\npm\bin\npm-cli.js'
node $npmCli ci --cache 'C:\Users\pc\Documents\Codex\tools\temperature\npm-cache' --no-audit --no-fund
node $npmCli test -- --runInBand
```

Pythonの標準ライブラリだけで旧形式の解析ができる。現在のparse_bin.pyは12バイトレコードを前提とする。欠損・ヘッダー・日時の扱いはISSUES.mdに残っているため、学習データを確定する前に元ログの整合性を検証する。

```powershell
$pythonExe = 'C:\Users\pc\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe'
& $pythonExe ai_training/parse_bin.py input.bin output.csv
& $pythonExe -m http.server 8765 --bind 127.0.0.1
```

ローカル画面は `http://127.0.0.1:8765/`。移行中は初期表示と設定を開閉する基本動作を確認する。カメラ・BLE・Gemini APIによる実処理は、接続・利用権限を確認した後に別途検証する。APIキーは文書やGitへ書かない。現行画面はブラウザlocalStorageへキーを保存する実装で、キー保存方法の見直しは既知課題に残す。

## Arduino

- Arduino CLI: 1.5.1。公式配布ZIPのSHA256確認済み。
- ボード: `esp32:esp32:XIAO_ESP32C3`。
- ESP32 core: **3.3.0**。monitorとai_collectorの両方でビルド成功。3.3.12/3.3.0の最初の失敗は長いGCCヘッダー検索パスで再現し、Arduino dataを短いパスへ移して解消した。
- 旧環境から確認して導入した版: U8g2 2.35.30、RTClib 2.1.4、Adafruit AHTX0 2.0.6、Adafruit BusIO 1.17.4、Adafruit Unified Sensor 1.1.15。
- 依存として導入: Adafruit SH110X 2.1.15、Adafruit GFX Library 1.12.6。
- 設定: `C:\Users\pc\Documents\Codex\tools\temperature\arduino-cli.yaml`。dataは `C:\Users\pc\Documents\Codex\mcu`、downloads/userはtemperatureフォルダ配下。GCCのヘッダー探索が長いパスで失敗するため、dataを旧パスへ戻さない。

```powershell
$arduinoCli = 'C:\Users\pc\Documents\Codex\tools\temperature\arduino-cli\1.5.1\arduino-cli.exe'
$cliConfig = 'C:\Users\pc\Documents\Codex\tools\temperature\arduino-cli.yaml'
$env:ARDUINO_DIRECTORIES_DATA = 'C:\Users\pc\Documents\Codex\mcu'
& $arduinoCli --config-file $cliConfig compile --fqbn esp32:esp32:XIAO_ESP32C3 --build-path build/monitor mcu_firmware/xiao_env_monitor
& $arduinoCli --config-file $cliConfig compile --fqbn esp32:esp32:XIAO_ESP32C3 --build-path build/collector mcu_firmware/xiao_env_ai_collector
```

compileは実機に書き込まない。現在の実機の版と接続確認は移行の必須条件から外して移行を完了したが、その後ユーザーが現在の実機を完成版の基準とすることを指定した。[実機基準の記録](DEVICE_BASELINE.md)を確認する。4 MB原本とLittleFSログを保全済み。今後増えるログは領域変更・消去前に追加保全する。

実機の開発用ライブラリ情報とBLEソースパスはcore **3.3.7**を示す。照合用に別dataフォルダ `C:\Users\pc\Documents\Codex\mcu-match-337` を用意し、公式core 3.3.7、ESP32-C3用ライブラリ、esp-rv32 2511、esptool 5.1.0で保存済みAI収集版をコンパイルした。core 3.3.0環境は保持した。生成アプリは実機と完全一致せず、元ソースとビルド設定の照合を続ける。compile成功だけで実機の完成版を再現済みとは扱わず、未確認の候補を実機へ書き込まない。

## Git

同梱GitのHTTPSヘルパーは、現在の環境では実行パスの指定が必要。

```powershell
$env:GIT_EXEC_PATH = 'C:\Users\pc\.cache\codex-runtimes\codex-primary-runtime\dependencies\native\git\mingw64\bin'
git fetch origin
git status --short
```

共同対応でCodex連携に温度計を追加・保存し、GitHubコネクターの移行ブランチ作成が成功した。コネクターのBlob/Tree/Commit/Ref操作でコードを反映し、取得し直した内容とSHAを照合できる。端末Gitでpushも使う場合は別途認証する。認証トークンをファイルへ転記しない。

Python等からGitを実行する際、sandboxと所有ユーザーの違いで安全なディレクトリ確認に失敗する場合は、確認済みのこのフォルダだけを `git -c safe.directory=C:/Users/pc/Documents/Codex/projects/xiao-scanner ...` で指定する。
