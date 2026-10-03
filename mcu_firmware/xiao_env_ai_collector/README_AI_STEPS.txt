XIAO ESP32C3 AI Data Collector — Codex引き継ぎ
更新: 2026-10-03

現行スケッチはv3.1で、model.hにTinyML予測モデルを含む。
通常の開発入口はCodex。リポジトリ直下のREADME.md、HANDOVER.md、
ISSUES.mdとdocs/DEVELOPMENT.mdを参照する。

使用するファイル:
- xiao_env_ai_collector.ino: 収集・表示・BLE・LittleFS・予測処理
- model.h: 既存モデル。移行時の再学習や差し替えは行わない
- qrcode.c / qrcode.h: このスケッチに同梱するQR実装
- ai_training/Colab_Training_Guide.md: 既存の学習資料
- ai_training/parse_bin.py: 旧12バイトログ用パーサー

AIログ:
温度・湿度・DIのfloat三つを12バイト/件で保存し、最大44,640件。
現行WebのGET_AI_LOGに対応するが、転送・時系列・完全性の課題が残る。
16バイトヘッダー/AI_HDRに対応する未完成のWeb/Python作業版は
archives/antigravity-2026-05-21に保存し、現行実装へ上書きしない。

次の学習・不具合修正:
まず実機に書き込まれた版を確認し、元ログと受信ログの整合性を検証する。
TinyMLモデルの学習元・再生成条件は既存資料とモデルを照合して確定する。
旧手順の「GET_AI_LOGは今後追加」「モデルは今後導入」は現状と異なる。
移行前の資料はdocs/history/README_AI_STEPS-before-migration.txtへ保持。

ビルドはdocs/DEVELOPMENT.mdのArduino CLI手順で行う。
実機への書き込みやログ消去は、版とログ保全を確認する共同対応で扱う。
