# XIAO ESP32C3 - AIモデル(TinyML)学習ガイド

Google Colabを使って、収集した環境データ(`.bin`)からマイコン用の予測AIモデル(`model.h`)を作成する手順です。

## 1. Google Colabを開く
1. ブラウザで [Google Colab (https://colab.research.google.com/)](https://colab.research.google.com/) にアクセスします。
2. Googleアカウントでログインし、**「ノートブックを新規作成」** をクリックします。

## 2. データのアップロード
1. 画面左側の **フォルダアイコン（ファイル）** をクリックします。
2. そのスペースに、PCに保存した **`ai_env_log_1778673651.bin`** をドラッグ＆ドロップしてアップロードします。（アップロード完了まで数十秒お待ちください）

## 3. 学習コードの実行
Colabの入力枠（セル）に以下のコードをすべてコピー＆ペーストし、左側の「▶（実行ボタン）」を押します。

```python
!pip install micromlgen scikit-learn numpy

import struct
import numpy as np
from sklearn.ensemble import RandomForestRegressor
from micromlgen import port

# --- 1. バイナリデータの読み込み ---
filename = "ai_env_log_1778673651.bin"
print(f"Reading {filename}...")

with open(filename, "rb") as f:
    data = f.read()

record_size = 12 # float(4bytes) x 3
num_records = len(data) // record_size
records = []

for i in range(num_records):
    offset = i * record_size
    # リトルエンディアンで Temp, Hum, DI を読み出し
    t, h, di = struct.unpack('<fff', data[offset:offset+record_size])
    records.append([t, h, di])

records = np.array(records)
print(f"Loaded {len(records)} records (approx {len(records)/60:.1f} hours of data).")

# --- 2. AI学習用データの作成 ---
# 「過去2時間(120分)のデータ」から、「1時間(60分)後の温度」を予測します
HISTORY_LEN = 120
FUTURE_OFFSET = 60

X = []
y = []

for i in range(len(records) - HISTORY_LEN - FUTURE_OFFSET):
    # 過去120分のデータを1列(360個の数字)に平坦化
    x_data = records[i : i + HISTORY_LEN].flatten()
    # 60分後の温度(Temp)を正解とする
    y_data = records[i + HISTORY_LEN + FUTURE_OFFSET][0]
    
    X.append(x_data)
    y.append(y_data)

X = np.array(X)
y = np.array(y)
print(f"Generated {len(X)} training samples.")

# --- 3. ランダムフォレストによる機械学習 ---
print("Training AI model... (This will take a few moments)")
# マイコンのメモリに収まるよう、決定木10本、深さ制限5の軽量モデルにする
model = RandomForestRegressor(n_estimators=10, max_depth=5, random_state=42)
model.fit(X, y)

score = model.score(X, y)
print(f"Model accuracy (R2 Score): {score:.3f}")

# --- 4. C言語ヘッダ(model.h)への出力 ---
print("Converting model to C code for ESP32...")
c_code = port(model)

with open("model.h", "w") as f:
    f.write(c_code)

print("✅ 'model.h' successfully created!")
```

## 4. model.h のダウンロード
1. 実行が完了（✅ 'model.h' successfully created! が表示）すると、左側のファイル一覧に **`model.h`** が新しく作成されます。（表示されない場合は、ファイル一覧の上にある「更新アイコン」を押してください）
2. `model.h` の右側にある「︙」メニューから **「ダウンロード」** を選び、PCに保存します。

これでマイコンの脳となる「知能（モデル）」の完成です！
ダウンロードが終わったら、チャットで教えてください。マイコン（Arduino IDE）への組み込み方をご案内します。
