// ========================
// ====================================
// XIAO ESP32C3 環境モニター v2.0
// Hardware: XIAO ESP32C3 + Expansion Base + Grove AHT20
// Features: 温湿度/不快指数/トレンドグラフ/ブザー/QRエクスポート/BLE
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
#include "esp_idf_version.h"
#include "qrcode.h"
#include "driver/gpio.h"

// --- ピン定義 ---
#define BUTTON_PIN  3
#define BUZZER_PIN  5

// --- 設定 ---
#define MAX_HISTORY 120
#define BATTERY_MONITOR_ENABLED false
#if BATTERY_MONITOR_ENABLED
#define BATTERY_PIN A0
#endif

// --- グラフ描画定数 ---
#define GX_LEFT   28
#define GX_RIGHT  122
#define GY_TOP    14
#define GY_BOTTOM 57
#define GY_TLABEL 63

// --- RTCメモリ変数（ディープスリープ保持）---
RTC_DATA_ATTR float temp_history[MAX_HISTORY];
RTC_DATA_ATTR float hum_history[MAX_HISTORY];
RTC_DATA_ATTR float di_history[MAX_HISTORY];
RTC_DATA_ATTR int history_head = 0;
RTC_DATA_ATTR int history_count = 0;
RTC_DATA_ATTR int display_mode = 0;   // 0:NOW 1:Temp 2:Hum 3:DI 4:Export
RTC_DATA_ATTR int trend_range = 0;    // 0:24h 1:12h 2:6h
RTC_DATA_ATTR int backup_count = 0;
RTC_DATA_ATTR uint8_t alert_flags = 0;

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
      long ut = rx.toInt();
      if (ut > 1700000000) { rtc.adjust(DateTime(ut)); timeUpdated = true; }
    }
  }
};

// ============================================================
// Forward declarations
// ============================================================
void loadHistory();
void saveHistory();
void recordData(float t, float h);
float calcDI(float t, float h);
const char* getComfortLabel(float t, float h, float di);
void buzzTone(int freq, int dur, int count);
void checkBuzzerAlert(float t, float h, float di);
int compressHistoryData(uint8_t* buf);
void startBLEServer();
void sendBLEHistory();
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
    loadHistory();
  }

  delay(40); // AHT20安定化（データシート準拠）

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
    // --- タイマー起動 or 初回起動 ---
    recordData(t, h);
    float di = calcDI(t, h);
    checkBuzzerAlert(t, h, di);

    backup_count++;
    if (backup_count >= 10) { // 2時間ごとフラッシュ保存
      saveHistory();
      backup_count = 0;
    }
  }
  else if (wr == ESP_SLEEP_WAKEUP_GPIO) {
    // --- ボタン起動 ---
    u8g2.begin();
    startBLEServer();
    saveHistory();

    display_mode = (display_mode + 1) % 5;
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
            // 履歴リセット
            history_count = 0; history_head = 0;
            backup_count = 0; alert_flags = 0;
            memset(temp_history, 0, sizeof(temp_history));
            memset(hum_history, 0, sizeof(hum_history));
            memset(di_history, 0, sizeof(di_history));
            saveHistory();
            u8g2.clearBuffer();
            u8g2.setFont(u8g2_font_ncenB10_tr);
            u8g2.drawStr(5, 35, "Cleared!");
            u8g2.sendBuffer();
            delay(1500);
            display_mode = 0;
            drawScreen(display_mode, t, h);
            break;
          }
          if (millis() - abs_start > 120000) break;
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
            char rs[16]; sprintf(rs, "Range: %dh", rh[trend_range]);
            u8g2.drawStr(15, 35, rs);
            u8g2.sendBuffer();
            delay(800);
            drawScreen(display_mode, t, h);
          } else {
            // 短押し: モード切替
            display_mode = (display_mode + 1) % 5;
            drawScreen(display_mode, t, h);
          }
        }
        wake_time = millis();
        delay(50);
      }

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

      if (!deviceConnected && (millis() - wake_time > 5000)) break;
      if (millis() - abs_start > 120000) break;
      delay(10);
    }

    u8g2.setPowerSave(1);
    BLEDevice::deinit(true);
  }

  goToSleep();
}

void loop() {}

// ============================================================
// フラッシュ保存・復元
// ============================================================
void loadHistory() {
  prefs.begin("env", true);
  history_head  = prefs.getInt("hd", 0);
  history_count = prefs.getInt("cnt", 0);
  if (prefs.isKey("t")) prefs.getBytes("t", temp_history, sizeof(temp_history));
  if (prefs.isKey("h")) prefs.getBytes("h", hum_history, sizeof(hum_history));
  if (prefs.isKey("d")) prefs.getBytes("d", di_history, sizeof(di_history));
  prefs.end();
  if (history_head < 0 || history_head >= MAX_HISTORY) history_head = 0;
  if (history_count < 0 || history_count > MAX_HISTORY) history_count = 0;
}

void saveHistory() {
  prefs.begin("env", false);
  prefs.putInt("hd", history_head);
  prefs.putInt("cnt", history_count);
  prefs.putBytes("t", temp_history, sizeof(temp_history));
  prefs.putBytes("h", hum_history, sizeof(hum_history));
  prefs.putBytes("d", di_history, sizeof(di_history));
  prefs.end();
}

// ============================================================
// データ記録
// ============================================================
void recordData(float t, float h) {
  temp_history[history_head] = t;
  hum_history[history_head] = h;
  di_history[history_head] = calcDI(t, h);
  history_head = (history_head + 1) % MAX_HISTORY;
  if (history_count < MAX_HISTORY) history_count++;
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
// データ圧縮（QR/BLE転送用）
// ============================================================
int compressHistoryData(uint8_t* buf) {
  DateTime now = rtc.now();
  uint32_t ts = now.unixtime();
  buf[0] = (uint8_t)history_count;
  buf[1] = (uint8_t)history_head;
  memcpy(&buf[2], &ts, 4);
  int pos = 6;
  for (int i = 0; i < history_count; i++) {
    int idx = (history_head - history_count + i + MAX_HISTORY * 2) % MAX_HISTORY;
    buf[pos++] = constrain((int)((temp_history[idx] - 10.0f) * 5.0f), 0, 255);
  }
  for (int i = 0; i < history_count; i++) {
    int idx = (history_head - history_count + i + MAX_HISTORY * 2) % MAX_HISTORY;
    buf[pos++] = constrain((int)(hum_history[idx] * 2.55f), 0, 255);
  }
  for (int i = 0; i < history_count; i++) {
    int idx = (history_head - history_count + i + MAX_HISTORY * 2) % MAX_HISTORY;
    buf[pos++] = constrain((int)((di_history[idx] - 30.0f) * 4.0f), 0, 255);
  }
  return pos;
}

// ============================================================
// BLEサーバー
// ============================================================
void startBLEServer() {
  BLEDevice::init("XIAO_RTC_SYNC");
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService *pSvc = pServer->createService(SERVICE_UUID);

  // 時刻書き込み用
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

void sendBLEHistory() {
  if (!deviceConnected || !pTxCharacteristic) return;
  uint8_t comp[400];
  int len = compressHistoryData(comp);
  for (int i = 0; i < len; i += 20) {
    int chunk = min(20, len - i);
    pTxCharacteristic->setValue(&comp[i], chunk);
    pTxCharacteristic->notify();
    delay(50);
  }
}

// ============================================================
// 描画メイン
// ============================================================
void drawScreen(int mode, float t, float h) {
  if (mode == 0) drawNowScreen(t, h);
  else if (mode <= 3) drawGraphScreen(mode, t, h);
  else drawExportScreen();
}

// --- Mode 0: 現在値 ---
void drawNowScreen(float t, float h) {
  u8g2.clearBuffer();
  float di = calcDI(t, h);
  const char* label = getComfortLabel(t, h, di);

  char timeStr[16] = "--:--";
  DateTime now = rtc.now() + TimeSpan(0, 9, 0, 0);
  if (now.year() >= 2024 && now.year() < 2050)
    sprintf(timeStr, "%02d/%02d %02d:%02d", now.month(), now.day(), now.hour(), now.minute());

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

// --- Mode 1-3: グラフ ---
void drawGraphScreen(int mode, float t, float h) {
  u8g2.clearBuffer();

  int maxDisp[] = {120, 60, 30};
  int display_count = min(history_count, maxDisp[trend_range]);

  // タイトル
  int rangeH[] = {24, 12, 6};
  int elapsedH = (display_count * 12) / 60;
  const char* names[] = {"", "Temp", "Hum", "DI"};
  char title[24];
  sprintf(title, "%s (%dh)", names[mode], rangeH[trend_range]);

  char timeStr[16] = "--:--";
  DateTime now = rtc.now() + TimeSpan(0, 9, 0, 0);
  if (now.year() >= 2024 && now.year() < 2050)
    sprintf(timeStr, "%02d/%02d %02d:%02d", now.month(), now.day(), now.hour(), now.minute());

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
    int idx = (history_head - display_count + i + MAX_HISTORY * 2) % MAX_HISTORY;
    float v;
    if (mode == 1) v = temp_history[idx];
    else if (mode == 2) v = hum_history[idx];
    else v = di_history[idx];
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
  sprintf(s, "%.1f", maxv); u8g2.drawStr(0, GY_TOP, s);
  sprintf(s, "%.1f", minv); u8g2.drawStr(0, GY_BOTTOM, s);

  // 基準線
  drawValueRef(mode, minv, maxv);
  drawTimeMarkers(display_count);

  // 折れ線描画
  for (int i = 0; i < display_count - 1; i++) {
    int i1 = (history_head - display_count + i + MAX_HISTORY * 2) % MAX_HISTORY;
    int i2 = (history_head - display_count + i + 1 + MAX_HISTORY * 2) % MAX_HISTORY;
    int x1 = map(i, 0, display_count - 1, GX_LEFT, GX_RIGHT);
    int x2 = map(i + 1, 0, display_count - 1, GX_LEFT, GX_RIGHT);
    float v1, v2;
    if (mode == 1) { v1 = temp_history[i1]; v2 = temp_history[i2]; }
    else if (mode == 2) { v1 = hum_history[i1]; v2 = hum_history[i2]; }
    else { v1 = di_history[i1]; v2 = di_history[i2]; }
    int y1 = map((long)(v1*100), (long)(minv*100), (long)(maxv*100), GY_BOTTOM, GY_TOP);
    int y2 = map((long)(v2*100), (long)(minv*100), (long)(maxv*100), GY_BOTTOM, GY_TOP);
    y1 = constrain(y1, GY_TOP, GY_BOTTOM);
    y2 = constrain(y2, GY_TOP, GY_BOTTOM);
    u8g2.drawLine(x1, y1, x2, y2);
  }

  u8g2.sendBuffer();
}

// --- Mode 4: Export（QRアニメーション + BLEデータ転送）---
void drawExportScreen() {
  if (history_count < 1) {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_5x7_tr);
    u8g2.drawStr(10, 35, "No data to export");
    u8g2.sendBuffer();
    return;
  }

  // BLE送信（接続中なら）
  sendBLEHistory();

  // データ圧縮
  uint8_t comp[400];
  int compLen = compressHistoryData(comp);
  int totalPages = (compLen + 39) / 40;

  // 案内画面（5秒：撮影準備の猶予）
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(0, 7,  "DATA EXPORT");
  u8g2.drawStr(0, 20, "QR: Record video now");
  u8g2.drawStr(0, 32, "BLE: Connect app");
  char info[32];
  sprintf(info, "Pages:%d Data:%dpts", totalPages, history_count);
  u8g2.drawStr(0, 48, info);
  u8g2.drawStr(0, 60, "Starting in 5s...");
  u8g2.sendBuffer();
  delay(5000);

  // QRアニメーション（最大10周 or 120秒でタイムアウト）
  const int MAX_LOOPS   = 10;
  const unsigned long TIMEOUT_MS = 120000UL;
  unsigned long startMs = millis();

  for (int lp = 0; lp < MAX_LOOPS; lp++) {
    for (int page = 0; page < totalPages; page++) {
      if (digitalRead(BUTTON_PIN) == LOW) return;

      unsigned long elapsed = millis() - startMs;
      if (elapsed >= TIMEOUT_MS) goto export_done;

      // ページデータ構築
      uint8_t qrPayload[42];
      qrPayload[0] = (uint8_t)page;
      qrPayload[1] = (uint8_t)totalPages;
      int offset = page * 40;
      int chunk = min(40, compLen - offset);
      if (chunk > 0) memcpy(&qrPayload[2], &comp[offset], chunk);
      int payloadLen = 2 + max(0, chunk);

      // QR生成
      QRCode qr;
      uint8_t qrBuf[qrcode_getBufferSize(3)];
      qrcode_initBytes(&qr, qrBuf, 3, ECC_LOW, qrPayload, payloadLen);

      // OLED描画
      u8g2.clearBuffer();
      int sz = qr.size;
      int sc = 2;
      int px = sz * sc;
      int ox = (64 - px) / 2;
      int oy = (64 - px) / 2;

      // 白背景
      u8g2.setDrawColor(1);
      u8g2.drawBox(ox - 2, oy - 2, px + 4, px + 4);
      // 黒モジュール
      u8g2.setDrawColor(0);
      for (int y = 0; y < sz; y++)
        for (int x = 0; x < sz; x++)
          if (qrcode_getModule(&qr, x, y))
            u8g2.drawBox(ox + x * sc, oy + y * sc, sc, sc);

      // 右サイドの情報（残り時間つき）
      u8g2.setDrawColor(1);
      u8g2.setFont(u8g2_font_5x7_tr);
      char ps[16];
      sprintf(ps, "P%d/%d", page + 1, totalPages);
      u8g2.drawStr(68, 10, ps);
      int remSec = (int)((TIMEOUT_MS - elapsed) / 1000);
      sprintf(ps, "Lp%d %ds", lp + 1, remSec);
      u8g2.drawStr(68, 22, ps);
      u8g2.drawStr(68, 36, deviceConnected ? "BLE:OK" : "BLE:--");
      sprintf(ps, "%d pts", history_count);
      u8g2.drawStr(68, 50, ps);

      u8g2.sendBuffer();
      delay(500);
    }
  }

export_done:
  // 完了/タイムアウト画面
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB10_tr);
  u8g2.drawStr(10, 25, "EXPORT");
  u8g2.drawStr(10, 45, "DONE");
  u8g2.sendBuffer();
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

  // trend_rangeに応じた間隔: 24h→6h間隔, 12h→3h, 6h→1h
  int intervals[] = {360, 180, 60};
  int interval = intervals[trend_range];
  int majorInt = (trend_range == 0) ? 720 : 360; // 点線を引く間隔

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
      sprintf(lb, "%d", ref / 60);
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
    // 温度: 中央値に点線
    float mid = (maxv + minv) / 2.0f;
    int ym = map((long)(mid * 100), (long)(minv * 100), (long)(maxv * 100), GY_BOTTOM, GY_TOP);
    ym = constrain(ym, GY_TOP, GY_BOTTOM);
    drawDottedH(ym, GX_LEFT, GX_RIGHT);
    char s[8]; sprintf(s, "%.1f", mid);
    u8g2.setFont(u8g2_font_4x6_tr);
    u8g2.drawStr(0, ym + 3, s);
    u8g2.setFont(u8g2_font_5x7_tr);
  }
  else if (mode == 2) {
    // 湿度: 40%と70%の固定線
    float refs[] = {40.0f, 70.0f};
    for (int r = 0; r < 2; r++) {
      if (refs[r] >= minv && refs[r] <= maxv) {
        int yr = map((long)(refs[r] * 100), (long)(minv * 100), (long)(maxv * 100), GY_BOTTOM, GY_TOP);
        yr = constrain(yr, GY_TOP, GY_BOTTOM);
        drawDottedH(yr, GX_LEFT, GX_RIGHT);
        char s[6]; sprintf(s, "%d", (int)refs[r]);
        u8g2.setFont(u8g2_font_4x6_tr);
        u8g2.drawStr(0, yr + 3, s);
        u8g2.setFont(u8g2_font_5x7_tr);
      }
    }
  }
  else if (mode == 3) {
    // 不快指数: 75に固定線
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
// ディープスリープ（時刻整列型）
// ============================================================
void goToSleep() {
  uint64_t sleepUs = 720ULL * 1000000ULL; // デフォルト12分

  DateTime now = rtc.now() + TimeSpan(0, 9, 0, 0);
  if (now.year() >= 2024 && now.year() < 2050) {
    int cm = now.minute();
    int cs = now.second();
    int next = ((cm / 12) + 1) * 12;
    int wait = (next - cm) * 60 - cs;
    if (next >= 60) wait = (60 - cm + (next - 60)) * 60 - cs;
    if (wait < 60) wait = 60;
    sleepUs = (uint64_t)wait * 1000000ULL;
  }

  esp_sleep_enable_timer_wakeup(sleepUs);
  esp_deep_sleep_enable_gpio_wakeup(1ULL << BUTTON_PIN, ESP_GPIO_WAKEUP_GPIO_LOW);
  gpio_pullup_en((gpio_num_t)BUTTON_PIN);
  gpio_hold_en((gpio_num_t)BUTTON_PIN);
  esp_deep_sleep_start();
}
