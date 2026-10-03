# Antigravityの未反映作業版

Driveのmac移行データ/antigravity/scratch/2_xiao_env_scanner_systemから、2026-10-03に原本のバイト列で保存した作業版。通常のWeb公開・Python解析には使わない。

- index.html: Web作業版（2026-05-21 22:21:57 JST）。AE01の16バイトヘッダー認識、AI_HDR認識、サイズ比較、範囲チェック、古い順の集計がある。
- parse_bin.py: Python作業版（2026-05-21 22:22:23 JST）。16バイトヘッダー認識と範囲チェックがある。

記録されたmainは4e0392659ed472a34eaa94aa4e7875163b9e23e6。両ファイルはそのコミットと現在のmainに一致しない。現行collectorはヘッダー/AI_HDRを生成しないため、この作業版だけを現行実装へ取り込んでも整合するとは限らない。

Web側はサイズ不一致でも解析へ進み、除外後の有効件数を1440件で区切る。Python側は除外後にMinuteを振り直すため、元の時間間隔を失う。これらはISSUES.mdの後続修正で対応する。Driveの別コピーと全ファイル、差分、Git参照は整理前バックアップに保存済み。
