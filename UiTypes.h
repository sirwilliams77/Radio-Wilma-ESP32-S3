#pragma once
#include <stdint.h>

#ifndef PROGMEM
#define PROGMEM
#endif

// One glyph of an antialiased font: 4-bit alpha, two pixels per byte, row-major.
struct UiGlyph {
  uint32_t offset;     // byte offset into the font bitmap
  uint16_t codepoint;  // Unicode code point (fonts are sorted by it)
  uint8_t width;
  uint8_t height;
  int8_t xOffset;      // left edge relative to the pen position
  int8_t yOffset;      // top edge relative to the baseline (negative = above)
  uint8_t xAdvance;
};

struct UiFont {
  const UiGlyph *glyphs;
  const uint8_t *bitmap;
  uint16_t glyphCount;
  uint8_t ascent;
  uint8_t descent;
  uint8_t capHeight;
};

// 4-bit alpha mask (icons); tinted with any color when drawn.
struct UiMask {
  const uint8_t *bits;
  uint8_t width;
  uint8_t height;
};

// Image sizes used by the layout (must match tools/make_ui_assets.py).
static const int16_t LOGO_W = 88;
static const int16_t LOGO_H = 66;
static const int16_t COVER_W = 76;
static const int16_t COVER_H = 68;
