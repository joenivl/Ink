#pragma once
// Minimale nabootsing van de ESPHome-display-API, zodat ink_kalender.h op de
// pc gecompileerd kan worden. Elke tekenopdracht wordt als JSON-regel naar
// stdout geschreven; render.py maakt daar een PNG van.

#include <cstdint>
#include <cstdio>
#include <string>

#include "metrics.h"  // gegenereerd door render.py

namespace esphome {

struct Color {
  uint8_t r{0}, g{0}, b{0}, w{0};
  Color() = default;
  Color(uint8_t r, uint8_t g, uint8_t b, uint8_t w = 0) : r(r), g(g), b(b), w(w) {}
};

namespace display {

enum class TextAlign { TOP_LEFT, TOP_CENTER, TOP_RIGHT };

struct BaseFont {
  const char *naam;  // sleutel in metrics.h, bijv. "vet"
  int index;
};

inline int stub_tekstbreedte(const BaseFont *f, const char *s) {
  int w = 0;
  const auto *p = reinterpret_cast<const unsigned char *>(s);
  while (*p) {
    uint32_t cp;
    if (*p < 0x80) {
      cp = *p++;
    } else if ((*p & 0xE0) == 0xC0) {
      cp = ((p[0] & 0x1F) << 6) | (p[1] & 0x3F);
      p += 2;
    } else if ((*p & 0xF0) == 0xE0) {
      cp = ((p[0] & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F);
      p += 3;
    } else {
      cp = '?';
      p += 4;
    }
    w += metrics::breedte(f->index, cp);
  }
  return w;
}

inline std::string json_tekst(const char *s) {
  std::string r;
  for (const char *p = s; *p; p++) {
    if (*p == '"' || *p == '\\')
      r += '\\';
    r += *p;
  }
  return r;
}

class Display {
 public:
  void fill(Color c) { std::printf("{\"op\":\"fill\",\"c\":%d}\n", c.r); }
  void filled_rectangle(int x, int y, int w, int h, Color c) {
    std::printf("{\"op\":\"rect\",\"x\":%d,\"y\":%d,\"w\":%d,\"h\":%d,\"c\":%d,\"fill\":1}\n", x, y, w, h, c.r);
  }
  void rectangle(int x, int y, int w, int h, Color c) {
    std::printf("{\"op\":\"rect\",\"x\":%d,\"y\":%d,\"w\":%d,\"h\":%d,\"c\":%d,\"fill\":0}\n", x, y, w, h, c.r);
  }
  void line(int x1, int y1, int x2, int y2, Color c) {
    std::printf("{\"op\":\"line\",\"x\":%d,\"y\":%d,\"x2\":%d,\"y2\":%d,\"c\":%d}\n", x1, y1, x2, y2, c.r);
  }
  void horizontal_line(int x, int y, int w, Color c) { filled_rectangle(x, y, w, 1, c); }
  void vertical_line(int x, int y, int h, Color c) { filled_rectangle(x, y, 1, h, c); }
  void circle(int x, int y, int r, Color c) {
    std::printf("{\"op\":\"circle\",\"x\":%d,\"y\":%d,\"r\":%d,\"c\":%d,\"fill\":0}\n", x, y, r, c.r);
  }
  void filled_circle(int x, int y, int r, Color c) {
    std::printf("{\"op\":\"circle\",\"x\":%d,\"y\":%d,\"r\":%d,\"c\":%d,\"fill\":1}\n", x, y, r, c.r);
  }
  void print(int x, int y, BaseFont *f, Color c, TextAlign a, const char *s, Color = Color()) {
    std::printf("{\"op\":\"text\",\"x\":%d,\"y\":%d,\"font\":\"%s\",\"c\":%d,\"align\":%d,\"s\":\"%s\"}\n", x, y,
                f->naam, c.r, static_cast<int>(a), json_tekst(s).c_str());
  }
  void get_text_bounds(int x, int y, const char *s, BaseFont *f, TextAlign, int *x1, int *y1, int *w, int *h) {
    *x1 = x;
    *y1 = y;
    *w = stub_tekstbreedte(f, s);
    *h = metrics::hoogte(f->index);
  }
};

}  // namespace display
}  // namespace esphome
