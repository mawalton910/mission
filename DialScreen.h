#pragma once
#include <M5Dial.h>

// All text stays inside the usable circle, not the square framebuffer corners.
namespace DialScreen {
inline uint16_t bg() { return M5Dial.Display.color565(8, 17, 24); }
inline uint16_t muted() { return M5Dial.Display.color565(165, 187, 197); }
inline uint16_t accent() { return M5Dial.Display.color565(58, 224, 174); }
inline void line(String value, int y, int size = 1, uint16_t color = TFT_WHITE, int width = 190) {
  M5Dial.Display.setTextDatum(MC_DATUM);
  M5Dial.Display.setTextColor(color, bg());
  M5Dial.Display.setTextSize(size);
  while (M5Dial.Display.textWidth(value) > width && value.length() > 1) value.remove(value.length() - 1);
  M5Dial.Display.drawString(value, 120, y);
}
inline void wrap(String value, int y, int size = 2, int maxLines = 3, uint16_t color = TFT_WHITE, int width = 190) {
  M5Dial.Display.setTextSize(size);
  value.replace("\n", " "); value.replace("\\n", " "); value.trim();
  for (int row = 0; row < maxLines && value.length(); ++row) {
    int end = value.length();
    while (end > 1 && M5Dial.Display.textWidth(value.substring(0, end)) > width) --end;
    if (end < (int)value.length()) {
      int space = value.lastIndexOf(' ', end);
      if (space > end / 2) end = space;
    }
    String part = value.substring(0, end);
    value = value.substring(end); value.trim();
    if (row == maxLines - 1 && value.length()) { if (part.length() > 3) part.remove(part.length() - 3); part += "..."; }
    line(part, y + row * (size * 8 + 5), size, color, width);
  }
}
inline void base(const String& label, uint16_t color = accent()) {
  M5Dial.Display.setFont(&fonts::Font0);
  M5Dial.Display.fillScreen(bg());
  M5Dial.Display.drawRoundRect(51, 22, 138, 25, 12, color);
  line(label, 35, 1, color, 124);
}
inline void message(const String& label, const String& body, const String& footer = "", uint16_t color = accent()) {
  base(label, color);
  wrap(body, 91, 2, 4);
  wrap(footer, 185, 1, 2, muted(), 170);
}
}
