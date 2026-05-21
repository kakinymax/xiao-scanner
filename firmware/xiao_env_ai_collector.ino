// ========================
// ====================================
// XIAO ESP32C3 AI Data Collector v3.1 (TinyML Prediction)
// Hardware: XIAO ESP32C3 + Expansion Base + Grove AHT20
// Features: 温湿度/不快指数/トレンド表示/BLE + AI学習用1分ログ(LittleFS)
//           + TinyML(RandomForest)による1時間後温度予測
// ============================================================


#include <Wire.h>
#include <Adafruit_AHTX0.h>
#include <U8g2lib.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <RTClib.h>
#include <Preferences.h>
#include <LittleFS.h>
#include "esp_idf_version.h"
#include "qrcode.h"
#include "driver/gpio.h"
#include "model.h"  // TinyML予測モデル

// --- TinyMLオブジェクト ---
Eloquent::ML::Port::RandomForestRegressor mlModel;

// --- ピン定義 ---
#define BUTTON_PIN  3
#define BUZZER_PIN  5

// --- 設定 ---
// 表用（OLED/QR）: 12分おき × 120点 = 24時間分
#define UI_MAX_HISTORY 120
// 裏用（AIログ一時保持用）: 1分おき × 120点 = 2時間分
#define AI_MAX_BUFFER 120

// AIロギング定数
#define AI_LOG_FILE "/ai_env_log.bin"
#define AI_MAX_TOTAL_POINTS 44640 // 1ヶ月分 (31d * 24h * 60m)

// --- グラフ描画定数 ---
#define GX_LEFT   28
#define GX_RIGHT  122
#define GY_TOP    14
#define GY_BOTTOM 57
#define GY_TLABEL 63

// --- RTCメモリ変数（ディープスリープ保持）---
// 1. 表用データ (24H分の間引かれたデータ)
RTC_DATA_ATTR float ui_temp_history[UI_MAX_HISTORY];
RTC_DATA_ATTR float ui_hum_history[UI_MAX_HISTORY];
RTC_DATA_ATTR float ui_di_history[UI_MAX_HISTORY];
RTC_DATA_ATTR int ui_history_head = 0;
RTC_DATA_ATTR int ui_history_count = 0;

// 2. 裏用データ (2H分の一時バッファ)
RTC_DATA_ATTR float ai_temp_buffer[AI_MAX_BUFFER];
RTC_DATA_ATTR float ai_hum_buffer[AI_MAX_BUFFER];
RTC_DATA_ATTR float ai_di_buffer[AI_MAX_BUFFER];
RTC_DATA_ATTR int ai_buffer_count = 0;

// 3. 状態管理
RTC_DATA_ATTR int display_mode = 0;   // 0:NOW 1:Temp 2:Hum 3:DI 4:Export
RTC_DATA_ATTR int trend_range = 0;    // 0:24h 1:12h 2:6h
RTC_DATA_ATTR int alert_flags = 0;
RTC_DATA_ATTR int total_ai_points = 0; // LittleFSに書き込んだ累計点数
RTC_DATA_ATTR int ai_unsaved_count = 0; // 保存待ちの点数
RTC_DATA_ATTR int last_ui_minute_recorded = -1; // UIデータを最後に記録した「分」

// --- グローバルオブジェクト ---
Adafruit_AHTX0 aht;
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
RTC_PCF8563 rtc;
Preferences prefs;

// --- BLE ---
BLEServer* pServer = NULL;
BLECharacteristic* pTxCharacteristic = NULL;
bool deviceConnected = false;
bool timeUpdated = false;
bool aiTransferMode = false; // AIデータ全送信モード

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHAR_UUID_RX        "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define CHAR_UUID_TX        "beb5483e-36e1-4688-b7f5-ea07361b26a9"

class MyServerCallbacks: public BLEServerCallbacks {
  void onConnect(BLEServer* s)    { deviceConnected = true; }
  void onDisconnect(BLEServer* s) { deviceConnected = false; }
};

class MyCallbacks: public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pC) {
    String rx = pC->getValue();
    if (rx.length() > 0) {
      rx.trim();
      // コマンド処理
      if (rx == "GET_AI_LOG") {
        aiTransferMode = true; // AIログ送信要求フラグ
      } else {
        // 時刻同期処理
        long ut = rx.toInt();
        if (ut > 1700000000) { rtc.adjust(DateTime(ut)); timeUpdated = true; }
      }
    }
  }
};

// ============================================================
// Forward declarations
// ============================================================
void loadUIHistory();
void saveUIHistory();
void flushAIBufferToFS();
void recordData(float t, float h);
float calcDI(float t, float h);
const char* getComfortLabel(float t, float h, float di);
void buzzTone(int freq, int dur, int count);
void checkBuzzerAlert(float t, float h, float di);
int compressUIHistoryData(uint8_t* buf);
void startBLEServer();
void sendBLEUIHistory();
void sendBLEAILog();
void drawScreen(int mode, float t, float h);
void drawNowScreen(float t, float h);
void drawGraphScreen(int mode, float t, float h);
void drawExportScreen();
void drawDottedH(int y, int x0, int x1);
void drawDottedV(int x, int y0, int y1);
void drawTimeMarkers(int display_count);
void drawValueRef(int mode, float minv, float maxv);
void goToSleep();

// ============================================================
// SETUP
// ============================================================
void setup() {
  gpio_hold_dis((gpio_num_t)BUTTON_PIN);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  Wire.begin();
  rtc.begin();

  esp_sleep_wakeup_cause_t wr = esp_sleep_get_wakeup_cause();

  if (wr == ESP_SLEEP_WAKEUP_UNDEFINED) {
    // 初回起動時の読み出し
    loadUIHistory();
  }

  // LittleFSのマウント (失敗時はフォーマット)
  if (!LittleFS.begin(true)) {
    // フォーマットが行われた場合、累計点数をリセット
    total_ai_points = 0;
  } else {
    // 初回ではない場合、ファイルサイズから現在の蓄積点数を概算
    File file = LittleFS.open(AI_LOG_FILE, FILE_READ);
    if(file) {
      total_ai_points = file.size() / 12; // 12 bytes per record (3 floats)
      file.close();
    }
  }

  delay(40); // AHT20安定化

  if (!aht.begin()) {
    if (wr == ESP_SLEEP_WAKEUP_GPIO) {
      u8g2.begin();
      u8g2.clearBuffer();
      u8g2.setFont(u8g2_font_ncenB10_tr);
      u8g2.drawStr(10, 35, "Sensor Error!");
      u8g2.sendBuffer();
      delay(5000);
      u8g2.setPowerSave(1);
    }
    goToSleep();
  }

  sensors_event_t humidity, temp;
  aht.getEvent(&humidity, &temp);
  float t = temp.temperature;
  float h = humidity.relative_humidity;

  if (wr == ESP_SLEEP_WAKEUP_TIMER || wr == ESP_SLEEP_WAKEUP_UNDEFINED) {
    // --- タイマー起動 (1分おき) or 初回起動 ---
    
    // 1. データ記録メインルーチン (表・裏への振り分け)
    recordData(t, h);

    // 2. ブザー警告
    float di = calcDI(t, h);
    checkBuzzerAlert(t, h, di);

    // 3. 一括Flash保存 (2時間おき)
    if (ai_unsaved_count >= AI_MAX_BUFFER) { 
      flushAIBufferToFS(); // 裏ログをファイルへ
      saveUIHistory();     // 表ログをPreferencesへ
    }
  }
  else if (wr == ESP_SLEEP_WAKEUP_GPIO) {
    // --- ボタン起動 ---
    u8g2.begin();
    startBLEServer();
    
    // 手動起動時にも安全のため現在の状態を退避
    saveUIHistory();

    display_mode = 0; // 起動時は一番初めの画面（NOW画面）に戻る
    drawScreen(display_mode, t, h);

    unsigned long wake_time = millis();
    unsigned long abs_start = millis();
    delay(200);

    while (true) {
      if (digitalRead(BUTTON_PIN) == LOW) {
        unsigned long pressStart = millis();
        bool superLong = false;

        while (digitalRead(BUTTON_PIN) == LOW) {
          if (millis() - pressStart > 5000 && !superLong) {
            superLong = true;
            // 全履歴リセット (UI & AI)
            ui_history_count = 0; ui_history_head = 0;
            ai_buffer_count = 0;  alert_flags = 0; total_ai_points = 0;
            memset(ui_temp_history, 0, sizeof(ui_temp_history));
            memset(ui_hum_history, 0, sizeof(ui_hum_history));
            memset(ui_di_history, 0, sizeof(ui_di_history));
            saveUIHistory();
            LittleFS.remove(AI_LOG_FILE); // AIログファイル削除

            u8g2.clearBuffer();
            u8g2.setFont(u8g2_font_ncenB10_tr);
            u8g2.drawStr(5, 35, "All Cleared!");
            u8g2.sendBuffer();
            delay(1500);
            display_mode = 0;
            drawScreen(display_mode, t, h);
            break;
          }
          if (millis() - abs_start > 120000 && !aiTransferMode) break;
          delay(10);
        }

        if (!superLong) {
          unsigned long dur = millis() - pressStart;
          if (dur >= 2000) {
            // 長押し: トレンド期間切替
            trend_range = (trend_range + 1) % 3;
            u8g2.clearBuffer();
            u8g2.setFont(u8g2_font_ncenB10_tr);
            int rh[] = {24, 12, 6};
            char rs[16]; snprintf(rs, sizeof(rs), "Range: %dh", rh[trend_range]);
            u8g2.drawStr(15, 35, rs);
            u8g2.sendBuffer();
            delay(800);
            drawScreen(display_mode, t, h);
          } else {
            // 短押し: モード切替
            display_mode = (display_mode + 1) % 6; 
            drawScreen(display_mode, t, h);
            
            // Export画面から自動/手動でMode 0に戻った場合、即座に再描画
            if (display_mode == 0) drawScreen(display_mode, t, h);
          }
        }
        wake_time = millis(); // ボタン操作でスリープタイマーをリセット
        delay(50);
      }

      // スマホから時刻同期要求がきた
      if (timeUpdated) {
        u8g2.clearBuffer();
        u8g2.setFont(u8g2_font_ncenB10_tr);
        u8g2.drawStr(10, 35, "Time Synced!");
        u8g2.sendBuffer();
        delay(1500);
        timeUpdated = false;
        drawScreen(display_mode, t, h);
        wake_time = millis();
      }

      // スマホからAIバルク転送要求がきた
      if (aiTransferMode) {
        u8g2.clearBuffer();
        u8g2.setFont(u8g2_font_5x7_tr);
        u8g2.drawStr(10, 20, "AI Transfer Mode");
        u8g2.drawStr(10, 35, "Don't turn off...");
        u8g2.sendBuffer();
        
        sendBLEAILog(); // 時間がかかる処理
        aiTransferMode = false;
        
        u8g2.clearBuffer();
        u8g2.drawStr(10, 35, "Transfer DONE.");
        u8g2.sendBuffer();
        delay(2000);
        drawScreen(display_mode, t, h);
        wake_time = millis(); // 転送が終わったらタイムアウトをリセット
      }

      // スリープへの遷移判定 (転送中はスリープしない)
      if (!aiTransferMode) {
        if (millis() - wake_time > 60000) break; // 自動スリープを1分に変更
      }
      delay(10);
    }

    u8g2.setPowerSave(1);
    BLEDevice::deinit(true);
  }

  goToSleep();
}

void loop() {}

// ============================================================
// フラッシュ保存・復元 (Preferences = 表のデータ用)
// ============================================================
void loadUIHistory() {
  prefs.begin("env", true);
  ui_history_head  = prefs.getInt("hd", 0);
  ui_history_count = prefs.getInt("cnt", 0);
  if (prefs.isKey("t")) prefs.getBytes("t", ui_temp_history, sizeof(ui_temp_history));
  if (prefs.isKey("h")) prefs.getBytes("h", ui_hum_history, sizeof(ui_hum_history));
  if (prefs.isKey("d")) prefs.getBytes("d", ui_di_history, sizeof(ui_di_history));
  prefs.end();
  if (ui_history_head < 0 || ui_history_head >= UI_MAX_HISTORY) ui_history_head = 0;
  if (ui_history_count < 0 || ui_history_count > UI_MAX_HISTORY) ui_history_count = 0;
}

void saveUIHistory() {
  prefs.begin("env", false);
  prefs.putInt("hd", ui_history_head);
  prefs.putInt("cnt", ui_history_count);
  prefs.putBytes("t", ui_temp_history, sizeof(ui_temp_history));
  prefs.putBytes("h", ui_hum_history, sizeof(ui_hum_history));
  prefs.putBytes("d", ui_di_history, sizeof(ui_di_history));
  prefs.end();
}

// ============================================================
// フラッシュ保存 (LittleFS = 裏のAIデータ用)
// ============================================================
void flushAIBufferToFS() {
  if (ai_unsaved_count == 0) return;

  // ファイルを追記モードで開く
  File file = LittleFS.open(AI_LOG_FILE, FILE_APPEND);
  if (!file) {
    file = LittleFS.open(AI_LOG_FILE, FILE_WRITE); // なければ作成
    if(!file) return;
  }

  // 保存点数の決定 (最大バッファサイズ分)
  int count_to_write = min(ai_unsaved_count, AI_MAX_BUFFER);
  
  // 保存対象が限界を超えたらリセット
  if (total_ai_points + count_to_write > AI_MAX_TOTAL_POINTS) {
    file.close();
    LittleFS.remove(AI_LOG_FILE);
    file = LittleFS.open(AI_LOG_FILE, FILE_WRITE);
    total_ai_points = 0;
  }

  // 保存待ちのデータを末尾からさかのぼって書き込む
  int start_idx = ai_buffer_count - count_to_write;
  if (start_idx < 0) start_idx = 0;

  for (int i = start_idx; i < ai_buffer_count; i++) {
    file.write((uint8_t*)&ai_temp_buffer[i], sizeof(float));
    file.write((uint8_t*)&ai_hum_buffer[i], sizeof(float));
    file.write((uint8_t*)&ai_di_buffer[i], sizeof(float));
  }
  
  file.close();
  
  // 更新
  total_ai_points += count_to_write;
  ai_unsaved_count = 0; // 保存待ちカウントだけリセット（メモリ内のバッファは維持）
}

// ============================================================
// データ記録 (デュアル・ロギング)
// ============================================================
void recordData(float t, float h) {
  float di = calcDI(t, h);
  
  DateTime now = rtc.now() + TimeSpan(0, 9, 0, 0); // JST
  int current_min = now.minute();
  
  // 1. 裏 (AI用): 毎回(1分おき)記録
  if (ai_buffer_count < AI_MAX_BUFFER) {
    ai_temp_buffer[ai_buffer_count] = t;
    ai_hum_buffer[ai_buffer_count] = h;
    ai_di_buffer[ai_buffer_count] = di;
    ai_buffer_count++;
  } else {
    // メモリがいっぱいの場合は、古いものを捨てて詰める (リングバッファ)
    for (int i = 0; i < AI_MAX_BUFFER - 1; i++) {
      ai_temp_buffer[i] = ai_temp_buffer[i + 1];
      ai_hum_buffer[i] = ai_hum_buffer[i + 1];
      ai_di_buffer[i] = ai_di_buffer[i + 1];
    }
    ai_temp_buffer[AI_MAX_BUFFER - 1] = t;
    ai_hum_buffer[AI_MAX_BUFFER - 1] = h;
    ai_di_buffer[AI_MAX_BUFFER - 1] = di;
  }
  ai_unsaved_count++; // 保存待ちカウントを増やす

  // 2. 表 (UI用): 12分おきに記録 (0分, 12分, 24分...)
  if (current_min % 12 == 0) {
    // 同じ分に複数回記録されないようガード (手動連打防止)
    if (last_ui_minute_recorded != current_min) {
      ui_temp_history[ui_history_head] = t;
      ui_hum_history[ui_history_head] = h;
      ui_di_history[ui_history_head] = di;
      ui_history_head = (ui_history_head + 1) % UI_MAX_HISTORY;
      if (ui_history_count < UI_MAX_HISTORY) ui_history_count++;
      
      last_ui_minute_recorded = current_min;
    }
  }
}

// ============================================================
// 不快指数・快適度判定
// ============================================================
float calcDI(float t, float h) {
  return 0.81f * t + 0.01f * h * (0.99f * t - 14.3f) + 46.3f;
}

const char* getComfortLabel(float t, float h, float di) {
  if (di >= 85.0) return "DANGER!";
  if (di >= 80.0) return "HeatAlert";
  if (h < 40.0)   return "Dry!";
  if (h > 70.0)   return "Mold!";
  if (di >= 75.0)  return "Warm";
  if (di < 55.0)   return "Cold!";
  return "Comfort";
}

// ============================================================
// ブザー
// ============================================================
void buzzTone(int freq, int dur, int count) {
  for (int i = 0; i < count; i++) {
    tone(BUZZER_PIN, freq, dur);
    delay(dur + 100);
  }
  noTone(BUZZER_PIN);
}

void checkBuzzerAlert(float t, float h, float di) {
  // 危険 DI>=85
  if (di >= 85.0) {
    if (!(alert_flags & 0x02)) { alert_flags |= 0x02; buzzTone(3000, 300, 3); }
  } else { alert_flags &= ~0x02; }
  // 熱中症注意 DI>=80
  if (di >= 80.0 && di < 85.0) {
    if (!(alert_flags & 0x01)) { alert_flags |= 0x01; buzzTone(2000, 200, 2); }
  } else if (di < 78.0) { alert_flags &= ~0x01; }
  // カビ hum>=70
  if (h >= 70.0) {
    if (!(alert_flags & 0x04)) { alert_flags |= 0x04; buzzTone(1000, 300, 1); }
  } else if (h < 65.0) { alert_flags &= ~0x04; }
  // 乾燥 hum<=40
  if (h <= 40.0) {
    if (!(alert_flags & 0x08)) { alert_flags |= 0x08; buzzTone(1500, 200, 1); }
  } else if (h > 45.0) { alert_flags &= ~0x08; }
}

// ============================================================
// データ圧縮（QR/BLE転送用 - UI履歴24H限定）
// ============================================================
int compressUIHistoryData(uint8_t* buf) {
  DateTime now = rtc.now();
  uint32_t ts = now.unixtime();
  buf[0] = (uint8_t)ui_history_count;
  buf[1] = (uint8_t)ui_history_head;
  memcpy(&buf[2], &ts, 4);

  int pos_temp = 6;
  int pos_hum = 6 + ui_history_count;
  int pos_di = 6 + ui_history_count * 2;

  for (int i = 0; i < ui_history_count; i++) {
    int idx = (ui_history_head - ui_history_count + i + UI_MAX_HISTORY * 2) % UI_MAX_HISTORY;
    buf[pos_temp++] = constrain((int)((ui_temp_history[idx] - 10.0f) * 5.0f), 0, 255);
    buf[pos_hum++] = constrain((int)(ui_hum_history[idx] * 2.55f), 0, 255);
    buf[pos_di++] = constrain((int)((ui_di_history[idx] - 30.0f) * 4.0f), 0, 255);
  }
  return pos_di;
}

// ============================================================
// BLEサーバー
// ============================================================
void startBLEServer() {
  BLEDevice::init("XIAO_RTC_SYNC"); // 名前はそのまま
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService *pSvc = pServer->createService(SERVICE_UUID);

  // 時刻書き込み & コマンド受信用
  BLECharacteristic *pRx = pSvc->createCharacteristic(CHAR_UUID_RX, BLECharacteristic::PROPERTY_WRITE);
  pRx->setCallbacks(new MyCallbacks());

  // データ送信用
  pTxCharacteristic = pSvc->createCharacteristic(CHAR_UUID_TX,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  pTxCharacteristic->addDescriptor(new BLE2902());

  pSvc->start();
  pServer->getAdvertising()->addServiceUUID(SERVICE_UUID);
  pServer->getAdvertising()->start();
}

// 通常の同期送信（24H分の間引かれたデータ）
void sendBLEUIHistory() {
  if (!deviceConnected || !pTxCharacteristic) return;
  uint8_t comp[400];
  int len = compressUIHistoryData(comp);
  for (int i = 0; i < len; i += 20) {
    int chunk = min(20, len - i);
    pTxCharacteristic->setValue(&comp[i], chunk);
    pTxCharacteristic->notify();
    delay(50);
  }
}

// AI用全ログの生ファイル送信 (巨大データ)
void sendBLEAILog() {
  if (!deviceConnected || !pTxCharacteristic) return;

  File file = LittleFS.open(AI_LOG_FILE, FILE_READ);
  if (!file) {
    // ログがなければ修了通知として短い文字列を送るなど
    pTxCharacteristic->setValue("NO_AI_LOG");
    pTxCharacteristic->notify();
    return;
  }

  // 転送開始のメタデータ送信などの設計も可能ですが、
  // ここではチャンク分割してひたすら送る例
  uint8_t chunkBuf[20];
  while(file.available()){
    int bytesRead = file.read(chunkBuf, 20);
    pTxCharacteristic->setValue(chunkBuf, bytesRead);
    pTxCharacteristic->notify();
    // 連続で送りすぎるとパケット落ちするため少し待つ
    // BLE特性に合わせて調整が必要。(ここでは安全側に20ms)
    delay(20); 
  }
  file.close();
  
  // 終了マーカー
  delay(100);
  pTxCharacteristic->setValue("EOF");
  pTxCharacteristic->notify();
}

void drawPredictionScreen();

// ============================================================
// 描画メイン
// ============================================================
void drawScreen(int mode, float t, float h) {
  if (mode == 0) drawNowScreen(t, h);
  else if (mode <= 3) drawGraphScreen(mode, t, h);
  else if (mode == 4) drawPredictionScreen();
  else drawExportScreen();
}

// --- Mode 4: Prediction (TinyML) ---
void drawPredictionScreen() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x7_tr);

  // AIバッファに十分なデータがあるか確認（最低120分分必要）
  if (ai_buffer_count < 120) {
    u8g2.setFont(u8g2_font_ncenB10_tr);
    u8g2.drawStr(5, 20, "Prediction");
    u8g2.setFont(u8g2_font_5x7_tr);
    char buf[28];
    snprintf(buf, sizeof(buf), "Need %d more min", 120 - ai_buffer_count);
    u8g2.drawStr(5, 40, buf);
    u8g2.drawStr(5, 55, "Data collecting...");
    u8g2.sendBuffer();
    return;
  }

  // 入力ベクトルを構築（直近120分分のデータを平坦化: 120 x 3 = 360個）
  // AIバッファは新しいデータが末尾に追加されるので、最新120点を使う
  float input[360];
  int start = ai_buffer_count - 120; // バッファの末尾120点の開始インデックス
  for (int i = 0; i < 120; i++) {
    input[i * 3 + 0] = ai_temp_buffer[start + i]; // Temp
    input[i * 3 + 1] = ai_hum_buffer[start + i];  // Hum
    input[i * 3 + 2] = ai_di_buffer[start + i];   // DI
  }

  // 推論実行（1時間後の予測温度）
  float predicted_temp = mlModel.predict(input);

  // 現在の不快指数と予測DIを簡易計算
  float cur_t = ai_temp_buffer[ai_buffer_count - 1];
  float cur_h = ai_hum_buffer[ai_buffer_count - 1];
  float cur_di = ai_di_buffer[ai_buffer_count - 1];

  // 表示
  u8g2.setFont(u8g2_font_ncenB10_tr);
  u8g2.drawStr(0, 13, "AI Predict");

  u8g2.setFont(u8g2_font_5x7_tr);
  char buf[28];

  // 現在値
  snprintf(buf, sizeof(buf), "Now:  %.1fC %.0f%%", cur_t, cur_h);
  u8g2.drawStr(0, 28, buf);

  // 1時間後の予測温度
  snprintf(buf, sizeof(buf), "+1h:  %.1f C", predicted_temp);
  u8g2.setFont(u8g2_font_ncenB10_tr);
  u8g2.drawStr(0, 45, buf);

  // 上昇/下降トレンドのコメント
  u8g2.setFont(u8g2_font_5x7_tr);
  float diff = predicted_temp - cur_t;
  if (diff > 1.0f)       u8g2.drawStr(0, 60, "Trend: Rising");
  else if (diff < -1.0f) u8g2.drawStr(0, 60, "Trend: Falling");
  else                   u8g2.drawStr(0, 60, "Trend: Stable");

  u8g2.sendBuffer();
}

// --- Mode 0: 現在値 ---
void drawNowScreen(float t, float h) {
  u8g2.clearBuffer();
  float di = calcDI(t, h);
  const char* label = getComfortLabel(t, h, di);

  char timeStr[16] = "--:--";
  DateTime now = rtc.now() + TimeSpan(0, 9, 0, 0);
  if (now.year() >= 2024 && now.year() < 2050)
    snprintf(timeStr, sizeof(timeStr), "%02d/%02d %02d:%02d", now.month(), now.day(), now.hour(), now.minute());

  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(0, 7, label);
  int tW1 = u8g2.getStrWidth(timeStr);
  u8g2.drawStr(128 - tW1, 7, timeStr);

  u8g2.setFont(u8g2_font_ncenB12_tr);
  u8g2.setCursor(0, 28);
  u8g2.print("T:"); u8g2.print(t, 1); u8g2.print("C");
  u8g2.setCursor(0, 46);
  u8g2.print("H:"); u8g2.print(h, 1); u8g2.print("%");
  u8g2.setCursor(0, 63);
  u8g2.print("DI:"); u8g2.print(di, 1);

  u8g2.sendBuffer();
}

// --- Mode 1-3: グラフ (UI履歴を使用) ---
void drawGraphScreen(int mode, float t, float h) {
  u8g2.clearBuffer();

  int maxDisp[] = {120, 60, 30};
  int display_count = min(ui_history_count, maxDisp[trend_range]);

  // タイトル
  int rangeH[] = {24, 12, 6};
  const char* names[] = {"", "Temp", "Hum", "DI"};
  char title[24];
  snprintf(title, sizeof(title), "%s (%dh)", names[mode], rangeH[trend_range]);

  char timeStr[16] = "--:--";
  DateTime now = rtc.now() + TimeSpan(0, 9, 0, 0);
  if (now.year() >= 2024 && now.year() < 2050)
    snprintf(timeStr, sizeof(timeStr), "%02d/%02d %02d:%02d", now.month(), now.day(), now.hour(), now.minute());

  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(0, 7, title);
  int tW2 = u8g2.getStrWidth(timeStr);
  u8g2.drawStr(128 - tW2, 7, timeStr);

  if (display_count < 2) {
    u8g2.drawStr(15, 40, "Not enough data...");
    u8g2.sendBuffer();
    return;
  }

  // min/max算出
  float minv = 999, maxv = -999;
  for (int i = 0; i < display_count; i++) {
    int idx = (ui_history_head - display_count + i + UI_MAX_HISTORY * 2) % UI_MAX_HISTORY;
    float v;
    if (mode == 1) v = ui_temp_history[idx];
    else if (mode == 2) v = ui_hum_history[idx];
    else v = ui_di_history[idx];
    if (v < minv) minv = v;
    if (v > maxv) maxv = v;
  }
  if ((maxv - minv) < 2.0f) {
    float mid = (maxv + minv) / 2.0f;
    maxv = mid + 1.0f; minv = mid - 1.0f;
  }

  // Y軸ラベル
  char s[10];
  u8g2.setFont(u8g2_font_5x7_tr);
  snprintf(s, sizeof(s), "%.1f", maxv); u8g2.drawStr(0, GY_TOP, s);
  snprintf(s, sizeof(s), "%.1f", minv); u8g2.drawStr(0, GY_BOTTOM, s);

  drawValueRef(mode, minv, maxv);
  drawTimeMarkers(display_count);

  // 折れ線描画
  for (int i = 0; i < display_count - 1; i++) {
    int i1 = (ui_history_head - display_count + i + UI_MAX_HISTORY * 2) % UI_MAX_HISTORY;
    int i2 = (ui_history_head - display_count + i + 1 + UI_MAX_HISTORY * 2) % UI_MAX_HISTORY;
    int x1 = map(i, 0, display_count - 1, GX_LEFT, GX_RIGHT);
    int x2 = map(i + 1, 0, display_count - 1, GX_LEFT, GX_RIGHT);
    float v1, v2;
    if (mode == 1) { v1 = ui_temp_history[i1]; v2 = ui_temp_history[i2]; }
    else if (mode == 2) { v1 = ui_hum_history[i1]; v2 = ui_hum_history[i2]; }
    else { v1 = ui_di_history[i1]; v2 = ui_di_history[i2]; }
    int y1 = map((long)(v1*100), (long)(minv*100), (long)(maxv*100), GY_BOTTOM, GY_TOP);
    int y2 = map((long)(v2*100), (long)(minv*100), (long)(maxv*100), GY_BOTTOM, GY_TOP);
    y1 = constrain(y1, GY_TOP, GY_BOTTOM);
    y2 = constrain(y2, GY_TOP, GY_BOTTOM);
    u8g2.drawLine(x1, y1, x2, y2);
  }

  u8g2.sendBuffer();
}

// --- Mode 4: Export（QRアニメーション等はUI履歴のみ送信）---
void drawExportScreen() {
  if (ui_history_count < 1) {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_5x7_tr);
    u8g2.drawStr(10, 35, "No data to export");
    u8g2.sendBuffer();
    return;
  }

  // BLE送信(表データ。通常スマホアプリへの展開用)
  sendBLEUIHistory();

  uint8_t comp[400];
  int compLen = compressUIHistoryData(comp);
  int totalPages = (compLen + 39) / 40;

  bool manual_mode = false;
  int page = 0;
  unsigned long last_qr_change = millis();
  unsigned long last_interaction = millis();

  while (millis() - last_interaction < 60000) { // 60秒無操作でタイムアウト
    // ボタン操作判定
    if (digitalRead(BUTTON_PIN) == LOW) {
      delay(50); // デバウンス
      unsigned long press_start = millis();
      bool long_press = false;
      while (digitalRead(BUTTON_PIN) == LOW) {
        if (millis() - press_start > 1000) {
          long_press = true;
          break;
        }
        delay(10);
      }
      
      if (long_press) {
        // 長押しでエクスポートモード終了
        while (digitalRead(BUTTON_PIN) == LOW) delay(10); // 離すまで待機
        display_mode = 0; // モードをリセット
        return; 
      } else {
        if (!manual_mode) {
          // 初回切替: 手動モードに入り、ページ1(index 0)から開始
          manual_mode = true;
          page = 0;
        } else {
          // 手動モード中: 次ページへ
          page++;
          if (page >= totalPages) {
            // 最後のページまで表示し終えたら終了
            goto export_done;
          }
        }
        last_qr_change = millis();
        last_interaction = millis();
      }
    }

    // 自動モード時のページ切替 (2倍遅い = 1000ms)
    if (!manual_mode && (millis() - last_qr_change > 1000)) {
      page = (page + 1) % totalPages;
      last_qr_change = millis();
    }

    // QR描画用データ構築
    uint8_t qrPayload[42];
    qrPayload[0] = (uint8_t)page;
    qrPayload[1] = (uint8_t)totalPages;
    int offset = page * 40;
    int chunk = min(40, compLen - offset);
    if (chunk > 0) memcpy(&qrPayload[2], &comp[offset], chunk);
    int payloadLen = 2 + max(0, chunk);

    QRCode qr;
    uint8_t qrBuf[qrcode_getBufferSize(3)];
    qrcode_initBytes(&qr, qrBuf, 3, ECC_LOW, qrPayload, payloadLen);

    u8g2.clearBuffer();
    int sz = qr.size;
    int sc = 2;
    int px = sz * sc;
    int ox = (64 - px) / 2;
    int oy = (64 - px) / 2;

    u8g2.setDrawColor(1);
    u8g2.drawBox(ox - 2, oy - 2, px + 4, px + 4);
    u8g2.setDrawColor(0);
    for (int y = 0; y < sz; y++) {
      for (int x = 0; x < sz; x++) {
        if (qrcode_getModule(&qr, x, y)) u8g2.drawBox(ox + x * sc, oy + y * sc, sc, sc);
      }
    }

    u8g2.setDrawColor(1);
    u8g2.setFont(u8g2_font_5x7_tr);
    char ps[16];
    snprintf(ps, sizeof(ps), "P%d/%d", page + 1, totalPages);
    u8g2.drawStr(68, 10, ps);
    snprintf(ps, sizeof(ps), manual_mode ? "Manual" : "Auto");
    u8g2.drawStr(68, 22, ps);
    u8g2.drawStr(68, 36, deviceConnected ? "BLE:OK" : "BLE:--");
    snprintf(ps, sizeof(ps), "%d pts", ui_history_count);
    u8g2.drawStr(68, 50, ps);
    
    int remSec = (int)((60000 - (millis() - last_interaction)) / 1000);
    snprintf(ps, sizeof(ps), "%ds", remSec);
    u8g2.drawStr(68, 62, ps);

    u8g2.sendBuffer();

    delay(50);
  }

  // タイムアウト
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB10_tr);
  u8g2.drawStr(10, 35, "TIMEOUT");
  u8g2.sendBuffer();
  delay(1000);
  display_mode = 0; // モードをリセット
  return;

export_done:
  // 手動モードで全ページ表示完了
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB10_tr);
  u8g2.drawStr(10, 25, "EXPORT");
  u8g2.drawStr(10, 45, "DONE");
  u8g2.sendBuffer();
  delay(1500);
  display_mode = 0; // モードをリセット
}

// ============================================================
// グラフ補助描画
// ============================================================
void drawDottedH(int y, int x0, int x1) {
  for (int x = x0; x <= x1; x += 3) u8g2.drawPixel(x, y);
}

void drawDottedV(int x, int y0, int y1) {
  for (int y = y0; y <= y1; y += 3) u8g2.drawPixel(x, y);
}

void drawTimeMarkers(int display_count) {
  DateTime now = rtc.now() + TimeSpan(0, 9, 0, 0);
  if (now.year() < 2024 || now.year() >= 2050) return;

  int curMin = now.hour() * 60 + now.minute();
  int maxMin = display_count * 12;

  int intervals[] = {360, 180, 60};
  int interval = intervals[trend_range];
  int majorInt = (trend_range == 0) ? 720 : 360; 

  for (int ref = 0; ref < 1440; ref += interval) {
    int ago = curMin - ref;
    if (ago < 0) ago += 1440;
    if (ago >= maxMin || ago < 0) continue;

    float ptsAgo = (float)ago / 12.0f;
    int di = display_count - 1 - (int)ptsAgo;
    if (di < 0 || di >= display_count) continue;

    int x = map(di, 0, display_count - 1, GX_LEFT, GX_RIGHT);

    if (ref % majorInt == 0) {
      drawDottedV(x, GY_TOP, GY_BOTTOM);
      char lb[4];
      snprintf(lb, sizeof(lb), "%d", ref / 60);
      u8g2.setFont(u8g2_font_4x6_tr);
      u8g2.drawStr(x - 2, GY_TLABEL, lb);
      u8g2.setFont(u8g2_font_5x7_tr);
    } else {
      u8g2.drawLine(x, GY_BOTTOM - 3, x, GY_BOTTOM);
    }
  }
}

void drawValueRef(int mode, float minv, float maxv) {
  if (mode == 1) {
    float mid = (maxv + minv) / 2.0f;
    int ym = map((long)(mid * 100), (long)(minv * 100), (long)(maxv * 100), GY_BOTTOM, GY_TOP);
    ym = constrain(ym, GY_TOP, GY_BOTTOM);
    drawDottedH(ym, GX_LEFT, GX_RIGHT);
    char s[8]; snprintf(s, sizeof(s), "%.1f", mid);
    u8g2.setFont(u8g2_font_4x6_tr);
    u8g2.drawStr(0, ym + 3, s);
    u8g2.setFont(u8g2_font_5x7_tr);
  }
  else if (mode == 2) {
    float refs[] = {40.0f, 70.0f};
    for (int r = 0; r < 2; r++) {
      if (refs[r] >= minv && refs[r] <= maxv) {
        int yr = map((long)(refs[r] * 100), (long)(minv * 100), (long)(maxv * 100), GY_BOTTOM, GY_TOP);
        yr = constrain(yr, GY_TOP, GY_BOTTOM);
        drawDottedH(yr, GX_LEFT, GX_RIGHT);
        char s[6]; snprintf(s, sizeof(s), "%d", (int)refs[r]);
        u8g2.setFont(u8g2_font_4x6_tr);
        u8g2.drawStr(0, yr + 3, s);
        u8g2.setFont(u8g2_font_5x7_tr);
      }
    }
  }
  else if (mode == 3) {
    if (75.0f >= minv && 75.0f <= maxv) {
      int yr = map(7500L, (long)(minv * 100), (long)(maxv * 100), GY_BOTTOM, GY_TOP);
      yr = constrain(yr, GY_TOP, GY_BOTTOM);
      drawDottedH(yr, GX_LEFT, GX_RIGHT);
      u8g2.setFont(u8g2_font_4x6_tr);
      u8g2.drawStr(0, yr + 3, "75");
      u8g2.setFont(u8g2_font_5x7_tr);
    }
  }
}

// ============================================================
// ディープスリープ（1分おきに起床するよう変更）
// ============================================================
void goToSleep() {
  // 基本1分(60秒)スリープ
  uint64_t sleepUs = 60ULL * 1000000ULL; 

  DateTime now = rtc.now() + TimeSpan(0, 9, 0, 0); // JST
  if (now.year() >= 2024 && now.year() < 2050) {
    // 毎分00秒ジャストに起きるように補正
    int cs = now.second();
    int wait = 60 - cs;
    if (wait < 5) wait = 60; // 5秒未満しか余裕がない場合は1分丸ごと寝る
    sleepUs = (uint64_t)wait * 1000000ULL;
  }

  esp_sleep_enable_timer_wakeup(sleepUs);
  esp_deep_sleep_enable_gpio_wakeup(1ULL << BUTTON_PIN, ESP_GPIO_WAKEUP_GPIO_LOW);
  gpio_pullup_en((gpio_num_t)BUTTON_PIN);
  gpio_hold_en((gpio_num_t)BUTTON_PIN);
  esp_deep_sleep_start();
}
