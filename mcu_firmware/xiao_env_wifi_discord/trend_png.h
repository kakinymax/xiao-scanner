#pragma once
#include "thermo_core.h"
#include <cstdio>

namespace thermo {
constexpr int TREND_WIDTH = 640, TREND_HEIGHT = 400, TREND_STRIDE = 81;
constexpr size_t TREND_RAW_BYTES = TREND_STRIDE * TREND_HEIGHT;
constexpr size_t TREND_PNG_BYTES = TREND_RAW_BYTES + 68;
constexpr size_t TREND_BODY_BYTES = TREND_PNG_BYTES + 2048;
constexpr const char *TREND_BOUNDARY = "XiaoTrendBoundary1";
inline void putBig32(uint8_t *p, uint32_t n) { for (int i = 0; i < 4; ++i) p[i] = uint8_t(n >> (24 - 8 * i)); }

// Draw into the PNG's stored DEFLATE block: one bounded buffer, no second bitmap.
class TrendCanvas {
  uint8_t *pixels;
public:
  explicit TrendCanvas(uint8_t *raw) : pixels(raw) {
    for (int y = 0; y < TREND_HEIGHT; ++y) {
      pixels[y * TREND_STRIDE] = 0;
      std::memset(pixels + y * TREND_STRIDE + 1, 255, TREND_STRIDE - 1);
    }
  }
  void pixel(int x, int y) {
    if (x >= 0 && x < TREND_WIDTH && y >= 0 && y < TREND_HEIGHT)
      pixels[y * TREND_STRIDE + 1 + x / 8] &= uint8_t(~(128U >> (x % 8)));
  }
  void line(int x, int y, int endX, int endY, bool thick = false) {
    int dx = std::abs(endX - x), dy = -std::abs(endY - y), sx = x < endX ? 1 : -1, sy = y < endY ? 1 : -1;
    int error = dx + dy;
    for (;;) {
      pixel(x, y); if (thick) pixel(x, y + 1);
      if (x == endX && y == endY) break;
      int twice = error * 2;
      if (twice >= dy) { error += dy; x += sx; }
      if (twice <= dx) { error += dx; y += sy; }
    }
  }
  void text(int x, int y, const char *s, int scale = 1) {
    // Five-column, seven-row ASCII font; Japanese descriptions are in Discord.
    static constexpr uint8_t font[][5] = {
      {62,81,73,69,62},{0,66,127,64,0},{66,97,81,73,70},{33,65,69,75,49},{24,20,18,127,16},
      {39,69,69,69,57},{60,74,73,73,48},{1,113,9,5,3},{54,73,73,73,54},{6,73,73,41,30},
      {126,17,17,17,126},{127,73,73,73,54},{62,65,65,65,34},{127,65,65,34,28},{127,73,73,73,65},
      {127,9,9,9,1},{62,65,73,73,122},{127,8,8,8,127},{0,65,127,65,0},{32,64,65,63,1},
      {127,8,20,34,65},{127,64,64,64,64},{127,2,12,2,127},{127,4,8,16,127},{62,65,65,65,62},
      {127,9,9,9,6},{62,65,81,33,94},{127,9,25,41,70},{70,73,73,73,49},{1,1,127,1,1},
      {63,64,64,64,63},{31,32,64,32,31},{63,64,56,64,63},{99,20,8,20,99},{7,8,112,8,7},{97,81,73,69,67}
    };
    for (; *s; ++s, x += 6 * scale) {
      uint8_t special[5]{}; const uint8_t *columns = special;
      if (*s >= '0' && *s <= '9') columns = font[*s - '0'];
      else if (*s >= 'A' && *s <= 'Z') columns = font[*s - 'A' + 10];
      else if (*s == '-') special[0] = special[1] = special[2] = special[3] = special[4] = 8;
      else if (*s == '.') special[2] = 96;
      else if (*s == ':') special[2] = 54;
      else if (*s == '/') { special[0] = 64; special[1] = 32; special[2] = 16; special[3] = 8; special[4] = 4; }
      else if (*s == '%') { special[0] = 99; special[1] = 19; special[2] = 8; special[3] = 100; special[4] = 99; }
      for (int col = 0; col < 5; ++col) for (int row = 0; row < 7; ++row)
        if (columns[col] & (1U << row)) for (int a = 0; a < scale; ++a) for (int b = 0; b < scale; ++b)
          pixel(x + col * scale + a, y + row * scale + b);
    }
  }
};

inline void trendPanel(TrendCanvas &c, const History &history, uint32_t begin, uint32_t end, bool humidity) {
  constexpr int left = 72, right = 615;
  int top = humidity ? 260 : 85, bottom = humidity ? 365 : 190;
  float minimum = 999, maximum = -999, last = 0; size_t visible = 0;
  for (size_t i = 0; i < history.count; ++i) {
    const Point &p = history.points[i]; if (p.epoch < begin || p.epoch >= end) continue;
    float v = humidity ? p.humidity : p.temperature;
    if (v < minimum) minimum = v;
    if (v > maximum) maximum = v;
    last = v; ++visible;
  }
  char label[80]; c.text(left, top - 25, humidity ? "HUMIDITY %" : "TEMPERATURE C", 2);
  if (visible) {
    std::snprintf(label, sizeof(label), "MIN %.1f  MAX %.1f  LAST %.1f", minimum, maximum, last);
    c.text(300, top - 20, label);
  }
  float low = visible ? std::floor(minimum - 1) : humidity ? 0 : 0;
  float high = visible ? std::ceil(maximum + 1) : humidity ? 100 : 40;
  float span = humidity ? 20 : 10;
  if (high - low < span) { float middle = (high + low) / 2; low = std::floor(middle - span / 2); high = low + span; }
  if (humidity) { if (low < 0) low = 0; if (high > 100) high = 100; }
  for (int tick = 0; tick <= 4; ++tick) {
    int y = bottom - tick * (bottom - top) / 4;
    std::snprintf(label, sizeof(label), "%.0f", low + float(tick) * (high - low) / 4);
    c.text(10, y - 6, label, 2);
    for (int x = left; x <= right; x += 4) c.pixel(x, y);
    int x = left + tick * (right - left) / 4;
    for (int gridY = top; gridY <= bottom; gridY += 4) c.pixel(x, gridY);
    std::snprintf(label, sizeof(label), "%02u:%02u", unsigned(((begin + 32400) / 3600 + tick * 6) % 24), unsigned((begin / 60) % 60));
    if (tick == 4 && (begin + 32400) % 86400 == 0) std::snprintf(label, sizeof(label), "24:00");
    c.text(x - 15, bottom + 10, label);
  }
  c.line(left, top, left, bottom); c.line(left, bottom, right, bottom);
  if (!visible) { c.text(280, (top + bottom) / 2, "NO DATA", 2); return; }
  bool previous = false; Point old; int oldX = 0, oldY = 0;
  for (size_t i = 0; i < history.count; ++i) {
    const Point &p = history.points[i];
    if (p.epoch < begin || p.epoch >= end) { previous = false; continue; }
    int x = left + int(uint64_t(p.epoch - begin) * (right - left) / (end - begin));
    float value = humidity ? p.humidity : p.temperature;
    int y = bottom - int((value - low) * (bottom - top) / (high - low));
    c.line(x - 1, y, x + 1, y, true);
    if (previous && connectedPoints(old, p)) c.line(oldX, oldY, x, y, true);
    previous = true; old = p; oldX = x; oldY = y;
  }
}

inline size_t renderTrendPng(uint8_t *out, size_t capacity, const History &history,
                             uint32_t begin, uint32_t end, const char *rangeLabel) {
  if (!out || capacity < TREND_PNG_BYTES || !validEpoch(begin) || end <= begin || end - begin != 86400) return 0;
  const uint8_t signature[] = {137,80,78,71,13,10,26,10}; std::memcpy(out, signature, 8);
  putBig32(out + 8, 13); std::memcpy(out + 12, "IHDR", 4);
  putBig32(out + 16, TREND_WIDTH); putBig32(out + 20, TREND_HEIGHT);
  out[24] = 1; out[25] = 0; out[26] = out[27] = out[28] = 0;
  putBig32(out + 29, crc32(out + 12, 17));
  putBig32(out + 33, TREND_RAW_BYTES + 11); std::memcpy(out + 37, "IDAT", 4);
  out[41] = 0x78; out[42] = 0x01; out[43] = 1; // zlib + final uncompressed block
  out[44] = uint8_t(TREND_RAW_BYTES); out[45] = uint8_t(TREND_RAW_BYTES >> 8);
  out[46] = uint8_t(~TREND_RAW_BYTES); out[47] = uint8_t((~TREND_RAW_BYTES) >> 8);
  TrendCanvas canvas(out + 48);
  canvas.text(72, 10, "24H TREND - JST", 2); canvas.text(72, 34, rangeLabel);
  trendPanel(canvas, history, begin, end, false); trendPanel(canvas, history, begin, end, true);
  uint32_t a = 1, b = 0;
  for (size_t i = 0; i < TREND_RAW_BYTES; ++i) { a = (a + out[48 + i]) % 65521; b = (b + a) % 65521; }
  putBig32(out + 48 + TREND_RAW_BYTES, (b << 16) | a);
  putBig32(out + 52 + TREND_RAW_BYTES, crc32(out + 37, TREND_RAW_BYTES + 15));
  putBig32(out + 56 + TREND_RAW_BYTES, 0); std::memcpy(out + 60 + TREND_RAW_BYTES, "IEND", 4);
  putBig32(out + 64 + TREND_RAW_BYTES, crc32(out + 60 + TREND_RAW_BYTES, 4));
  return TREND_PNG_BYTES;
}

inline size_t trendMultipart(uint8_t *out, size_t capacity, const char *json, const History &history,
                            uint32_t begin, uint32_t end, const char *rangeLabel) {
  if (!out || capacity < TREND_BODY_BYTES || std::strlen(json) >= 1200) return 0;
  int prefix = std::snprintf(reinterpret_cast<char *>(out), capacity,
    "--%s\r\nContent-Disposition: form-data; name=\"payload_json\"\r\nContent-Type: application/json\r\n\r\n%s\r\n"
    "--%s\r\nContent-Disposition: form-data; name=\"files[0]\"; filename=\"trend.png\"\r\nContent-Type: image/png\r\n\r\n",
    TREND_BOUNDARY, json, TREND_BOUNDARY);
  if (prefix <= 0 || size_t(prefix) + TREND_PNG_BYTES + 64 > capacity) return 0;
  size_t png = renderTrendPng(out + prefix, capacity - size_t(prefix), history, begin, end, rangeLabel);
  if (!png) return 0;
  int suffix = std::snprintf(reinterpret_cast<char *>(out + prefix + png), capacity - size_t(prefix) - png,
    "\r\n--%s--\r\n", TREND_BOUNDARY);
  return suffix > 0 ? size_t(prefix) + png + size_t(suffix) : 0;
}
} // namespace thermo
