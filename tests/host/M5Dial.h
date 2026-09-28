#pragma once
#include <Arduino.h>
#include <sstream>
#include <iomanip>
constexpr int MC_DATUM = 0;
constexpr uint16_t TFT_WHITE = 0xffff;
namespace fonts { inline int Font0; }
struct DisplayStub {
  std::ostringstream svg;
  int size = 1;
  uint16_t color = TFT_WHITE;
  static std::string hex(uint16_t c) { std::ostringstream s; s << '#' << std::hex << std::setfill('0') << std::setw(2) << ((c >> 11) * 255 / 31) << std::setw(2) << (((c >> 5) & 63) * 255 / 63) << std::setw(2) << ((c & 31) * 255 / 31); return s.str(); }
  static std::string escape(const String& value) { String text = value; text.replace("&", "&amp;"); text.replace("<", "&lt;"); text.replace(">", "&gt;"); return text; }
  uint16_t color565(int r, int g, int b) { return ((r & 248) << 8) | ((g & 252) << 3) | ((b & 248) >> 3); }
  void setTextDatum(int) {}
  void setFont(const int*) {}
  void setTextColor(uint16_t c, uint16_t) { color = c; }
  void setTextSize(int s) { size = s; }
  int textWidth(const String& text) { return text.length() * size * 6; }
  void fillScreen(uint16_t c) { svg.str(""); svg.clear(); svg << "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 240 240'><defs><clipPath id='dial'><circle cx='120' cy='120' r='120'/></clipPath></defs><g clip-path='url(#dial)'><circle cx='120' cy='120' r='120' fill='" << hex(c) << "'/>"; }
  void drawString(const String& text, int x, int y) { svg << "<text x='" << x << "' y='" << y << "' text-anchor='middle' dominant-baseline='middle' font-family='monospace' font-size='" << size * 8 << "' fill='" << hex(color) << "'>" << escape(text) << "</text>"; }
  void drawRoundRect(int x, int y, int w, int h, int r, uint16_t c) { svg << "<rect x='"<<x<<"' y='"<<y<<"' width='"<<w<<"' height='"<<h<<"' rx='"<<r<<"' fill='none' stroke='"<<hex(c)<<"'/>"; }
  void fillCircle(int x, int y, int r, uint16_t c) { svg << "<circle cx='"<<x<<"' cy='"<<y<<"' r='"<<r<<"' fill='"<<hex(c)<<"'/>"; }
  void drawCircle(int x, int y, int r, uint16_t c) { svg << "<circle cx='"<<x<<"' cy='"<<y<<"' r='"<<r<<"' fill='none' stroke='"<<hex(c)<<"'/>"; }
};
struct EncoderStub { long value = 0; long read() { return value; } };
struct M5Stub { DisplayStub Display; EncoderStub Encoder; };
inline M5Stub M5Dial;
