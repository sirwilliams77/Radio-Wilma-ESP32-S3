#include "RadioUi.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef ARDUINO
#include <Arduino_GFX_Library.h>
#include <esp_heap_caps.h>
#else
#include "host_gfx.h"  // desktop preview build only
#endif

#include "UiFonts.h"
#include "UiIcons.h"
#include "UiImages.h"
#include "UiWeather.h"

// ---------------------------------------------------------------------------
// Colors and layout (480x320, matched to the design mock-up scaled by 1/3.2)
// ---------------------------------------------------------------------------
static constexpr uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
  return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}
static const uint16_t COL_BG = 0x0000;
static const uint16_t COL_DIM = rgb(80, 86, 96);     // inactive signal bars / no Wi-Fi icon
static const uint16_t COL_RED = rgb(235, 80, 80);    // no Wi-Fi, MUTE

// User-adjustable colours (web panel: Ustawienia > Wygląd), 0xRRGGBB defaults.
const uint32_t UI_DEFAULT_COLORS[UI_C_COUNT] = {
    0xF5F7FA,  // UI_C_CLOCK
    0xF5F7FA,  // UI_C_COLON
    0xF5F7FA,  // UI_C_DATE
    0x16A8F0,  // UI_C_DATE_ACCENT
    0x16A8F0,  // UI_C_ACCENT (Wi-Fi, signal, PAUZA)
    0xF5F7FA,  // UI_C_VOLUME
    0x16A8F0,  // UI_C_STATION
    0x969CA6,  // UI_C_DESC
    0x969CA6,  // UI_C_LABEL
    0xF5F7FA,  // UI_C_ARTIST
    0x28BAEE,  // UI_C_TITLE
    0xF5F7FA,  // UI_C_TEMP
    0xDEE2E8,  // UI_C_CITY
    0xDEE2E8,  // UI_C_BOTTOM
    0x3A404C,  // UI_C_DIVIDER
    0x2ECC40,  // UI_C_EQ_LOW
    0xFACC15,  // UI_C_EQ_MID
    0xEF4444,  // UI_C_EQ_HIGH
    0x1678B4,  // UI_C_FRAME (rounded frames around the clock, station and track)
};
static uint16_t C[UI_C_COUNT];
static uint16_t rgb888(uint32_t c) { return rgb((c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF); }

static const int16_t SCREEN_W = 480;
static const int16_t MARGIN = 16;

static const int16_t TOP_H = 36;
static const int16_t TOP_BASELINE = 25;
static const int16_t TOP_DIVIDER_Y = 37;

static const int16_t CLOCK_X = 116, CLOCK_Y = 44, CLOCK_W = 249, CLOCK_H = 73;  // inside its frame
static const int16_t CLOCK_BASELINE = 69;  // inside the clock region
static const int16_t CLOCK_COLON_GAP = 8;
static const int16_t CLOCK_DIGIT_GAP = 5;

static const int16_t LOGO_X = 14, LOGO_Y = 130;  // centred in its frame
static const int16_t STATION_TEXT_X = 118, STATION_TEXT_W = SCREEN_W - 14 - STATION_TEXT_X;
static const int16_t COVER_X = 20, COVER_Y = 208;  // vinyl record, centred in its frame
static const int16_t TRACK_TEXT_X = 118, TRACK_TEXT_W = 198;

// Spectrum analyzer (segment style) to the right of the track block.
static const int16_t EQ_X = 328, EQ_Y = 207, EQ_W = 141, EQ_H = 71;  // inside its frame
static const uint8_t EQ_BANDS = 15;
static const int16_t EQ_BAR_W = 7, EQ_BAR_GAP = 2;
static const int16_t EQ_SEGMENT = 4, EQ_SEGMENT_STEP = 6;
static const int16_t EQ_BOTTOM = EQ_Y + EQ_H - 5;  // lowest segment row (screen y)
static const int16_t EQ_RANGE = 66;                // pixels from silence to full scale

// Weather: temperature and place to the left of the clock, icon (UiWeather.h) to the right.
static const int16_t WX_TEXT_X = 11, WX_TEXT_W = 94;  // inside its frame
static const int16_t WX_ICON_X = 374, WX_ICON_AREA_W = 95;
static const int16_t WX_Y = 44, WX_H = 73;

static const int16_t BOTTOM_DIVIDER_Y = 281;
static const int16_t BOTTOM_Y = 284, BOTTOM_H = 36;
static const int16_t BOTTOM_BASELINE = 21;  // inside the bottom region

static const uint32_t SCROLL_STEP_MS = 30;
static const uint32_t SCROLL_PAUSE_MS = 2500;
static const int16_t SCROLL_GAP = 48;

// ---------------------------------------------------------------------------
// Off-screen surface: one shared buffer, one region at a time
// ---------------------------------------------------------------------------
static Arduino_GFX *gfx = nullptr;
static uint16_t *surface = nullptr;
static const uint32_t SURFACE_PIXELS = 480UL * 80UL;
static int16_t surfX = 0, surfY = 0, surfW = 0, surfH = 0;

static void beginSurface(int16_t x, int16_t y, int16_t w, int16_t h) {
  surfX = x;
  surfY = y;
  surfW = w;
  surfH = h;
  memset(surface, 0, (size_t)w * h * sizeof(uint16_t));
}

// Copy of the whole screen (480x320 RGB565 in PSRAM) for the live preview in the web panel.
static const int16_t SCREEN_H = 320;
static uint16_t *shadow = nullptr;
static uint32_t shadowVersion = 0;
static uint32_t shadowRowVersion[SCREEN_H] = {0};  // version of the last change of each row

static void markShadowRows(int16_t y, int16_t h) {
  ++shadowVersion;
  for (int16_t yy = y < 0 ? 0 : y; yy < y + h && yy < SCREEN_H; ++yy) shadowRowVersion[yy] = shadowVersion;
}

static void shadowBlit(int16_t x, int16_t y, const uint16_t *pixels, int16_t w, int16_t h) {
  if (shadow == nullptr) return;
  for (int16_t row = 0; row < h; ++row) {
    const int16_t yy = y + row;
    if (yy < 0 || yy >= SCREEN_H) continue;
    for (int16_t col = 0; col < w; ++col) {
      const int16_t xx = x + col;
      if (xx >= 0 && xx < SCREEN_W) shadow[(uint32_t)yy * SCREEN_W + xx] = pixels[(uint32_t)row * w + col];
    }
  }
  markShadowRows(y, h);
}

static void shadowFill(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
  if (shadow == nullptr) return;
  for (int16_t yy = y; yy < y + h; ++yy) {
    if (yy < 0 || yy >= SCREEN_H) continue;
    for (int16_t xx = x; xx < x + w; ++xx) {
      if (xx >= 0 && xx < SCREEN_W) shadow[(uint32_t)yy * SCREEN_W + xx] = color;
    }
  }
  markShadowRows(y, h);
}

// Temporary full-screen views (encoders): while one is shown the main screen keeps its
// state but nothing of it is sent to the display; it is redrawn completely afterwards.
enum UiOverlay : uint8_t { OVERLAY_NONE, OVERLAY_STATIONS, OVERLAY_VOLUME, OVERLAY_SETUP };
static uint8_t overlayMode = OVERLAY_NONE;

static void pushSurface() {
  gfx->draw16bitRGBBitmap(surfX, surfY, surface, surfW, surfH);
  shadowBlit(surfX, surfY, surface, surfW, surfH);
}

static void flushSurface() {
  if (overlayMode != OVERLAY_NONE) return;  // main screen hidden
  pushSurface();
}

static inline uint16_t blend565(uint16_t bg, uint16_t fg, uint8_t alpha) {
  if (alpha >= 15) return fg;
  if (alpha == 0) return bg;
  const int rb = (bg >> 11) & 0x1F, gb = (bg >> 5) & 0x3F, bb = bg & 0x1F;
  const int rf = (fg >> 11) & 0x1F, gf = (fg >> 5) & 0x3F, bf = fg & 0x1F;
  const int r = rb + (rf - rb) * alpha / 15;
  const int g = gb + (gf - gb) * alpha / 15;
  const int b = bb + (bf - bb) * alpha / 15;
  return (uint16_t)((r << 11) | (g << 5) | b);
}

static void surfFillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
  x -= surfX;
  y -= surfY;
  for (int16_t yy = y; yy < y + h; ++yy) {
    if (yy < 0 || yy >= surfH) continue;
    for (int16_t xx = x; xx < x + w; ++xx) {
      if (xx >= 0 && xx < surfW) surface[(uint32_t)yy * surfW + xx] = color;
    }
  }
}

// Draws a 4-bit alpha mask (glyph or icon) at absolute screen coordinates.
static void surfDrawMask(const uint8_t *bits, int16_t w, int16_t h, int16_t x, int16_t y,
                         uint16_t color) {
  x -= surfX;
  y -= surfY;
  uint32_t pixel = 0;
  for (int16_t row = 0; row < h; ++row) {
    const int16_t yy = y + row;
    for (int16_t col = 0; col < w; ++col, ++pixel) {
      const uint8_t packed = bits[pixel >> 1];
      const uint8_t alpha = (pixel & 1) ? (packed & 0x0F) : (packed >> 4);
      const int16_t xx = x + col;
      if (alpha == 0 || yy < 0 || yy >= surfH || xx < 0 || xx >= surfW) continue;
      uint16_t &dst = surface[(uint32_t)yy * surfW + xx];
      dst = blend565(dst, color, alpha);
    }
  }
}

static void surfDrawIcon(const UiMask &icon, int16_t x, int16_t y, uint16_t color) {
  surfDrawMask(icon.bits, icon.width, icon.height, x, y, color);
}

// ---------------------------------------------------------------------------
// UTF-8 text
// ---------------------------------------------------------------------------
static uint16_t nextCodepoint(const char *&p) {
  const uint8_t c = (uint8_t)*p;
  if (c == 0) return 0;
  ++p;
  if (c < 0x80) return c;
  if ((c & 0xE0) == 0xC0 && ((uint8_t)p[0] & 0xC0) == 0x80) {
    const uint16_t cp = ((c & 0x1F) << 6) | ((uint8_t)p[0] & 0x3F);
    p += 1;
    return cp;
  }
  if ((c & 0xF0) == 0xE0 && ((uint8_t)p[0] & 0xC0) == 0x80 && ((uint8_t)p[1] & 0xC0) == 0x80) {
    const uint16_t cp = ((c & 0x0F) << 12) | (((uint8_t)p[0] & 0x3F) << 6) | ((uint8_t)p[1] & 0x3F);
    p += 2;
    return cp;
  }
  while (((uint8_t)*p & 0xC0) == 0x80) ++p;  // skip a broken or 4-byte sequence
  return '?';
}

static const UiGlyph *findGlyph(const UiFont &font, uint16_t cp) {
  int lo = 0, hi = (int)font.glyphCount - 1;
  while (lo <= hi) {
    const int mid = (lo + hi) / 2;
    const uint16_t v = font.glyphs[mid].codepoint;
    if (v == cp) return &font.glyphs[mid];
    if (v < cp) lo = mid + 1;
    else hi = mid - 1;
  }
  return nullptr;
}

static const UiGlyph *glyphOrFallback(const UiFont &font, uint16_t cp) {
  const UiGlyph *g = findGlyph(font, cp);
  return g ? g : findGlyph(font, '?');
}

static int16_t textWidth(const UiFont &font, const char *text, int8_t spacing = 0) {
  int16_t width = 0;
  bool first = true;
  for (const char *p = text; *p;) {
    const UiGlyph *g = glyphOrFallback(font, nextCodepoint(p));
    if (!g) continue;
    if (!first) width += spacing;
    width += g->xAdvance;
    first = false;
  }
  return width;
}

// Draws text with its pen starting at x; returns the pen position after the text.
static int16_t drawText(const UiFont &font, const char *text, int16_t x, int16_t baseline,
                        uint16_t color, int8_t spacing = 0) {
  for (const char *p = text; *p;) {
    const UiGlyph *g = glyphOrFallback(font, nextCodepoint(p));
    if (!g) continue;
    if (x >= surfX + surfW) break;
    if (g->width && x + g->xOffset + g->width > surfX) {
      surfDrawMask(font.bitmap + g->offset, g->width, g->height, x + g->xOffset,
                   baseline + g->yOffset, color);
    }
    x += g->xAdvance + spacing;
  }
  return x;
}

// Copies text into out, cut on a character boundary and ended with "…" if needed.
static void ellipsize(const UiFont &font, const char *text, int16_t maxWidth, char *out,
                      size_t outSize) {
  strncpy(out, text, outSize - 1);
  out[outSize - 1] = '\0';
  if (textWidth(font, out) <= maxWidth) return;
  const char *ellipsis = "\xE2\x80\xA6";
  const int16_t budget = maxWidth - textWidth(font, ellipsis);
  int16_t width = 0;
  const char *p = text;
  const char *cut = text;
  while (*p) {
    const char *before = p;
    const UiGlyph *g = glyphOrFallback(font, nextCodepoint(p));
    const int16_t adv = g ? g->xAdvance : 0;
    if (width + adv > budget) {
      cut = before;
      break;
    }
    width += adv;
    cut = p;
  }
  size_t len = (size_t)(cut - text);
  while (len > 0 && text[len - 1] == ' ') --len;
  if (len + 4 > outSize) len = outSize - 4;
  memcpy(out, text, len);
  memcpy(out + len, ellipsis, 4);  // 3 bytes + terminator
}

// ---------------------------------------------------------------------------
// Single text line with automatic marquee scrolling when it does not fit
// ---------------------------------------------------------------------------
struct TextLine {
  int16_t x, y, w, h;
  int16_t baseline;  // absolute screen y
  const UiFont *font;
  uint16_t color;
  char text[160];
  int16_t width;
  int16_t offset;
  bool scrolling;
  uint32_t nextStepAt;
};

// Station name is centred vertically between the label and the description.
static const int16_t STATION_BASELINE_LARGE = 176, STATION_BASELINE_SMALL = 172;
static TextLine stationLine = {STATION_TEXT_X, 141, STATION_TEXT_W, 44, STATION_BASELINE_LARGE, &FONT_STATION,
                               0, "", 0, 0, false, 0};
static TextLine descriptionLine = {STATION_TEXT_X, 185, STATION_TEXT_W, 17, 198, &FONT_SMALL,
                                   0, "", 0, 0, false, 0};
static TextLine artistLine = {TRACK_TEXT_X, 222, TRACK_TEXT_W, 28, 243, &FONT_ARTIST,
                              0, "", 0, 0, false, 0};
static TextLine titleLine = {TRACK_TEXT_X, 250, TRACK_TEXT_W, 27, 268, &FONT_TITLE,
                             0, "", 0, 0, false, 0};

static void renderLine(TextLine &line) {
  beginSurface(line.x, line.y, line.w, line.h);
  if (!line.scrolling) {  // fits: centered in its field
    drawText(*line.font, line.text, line.x + (line.w - line.width) / 2, line.baseline, line.color);
  } else {
    const int16_t start = line.x - line.offset;
    drawText(*line.font, line.text, start, line.baseline, line.color);
    drawText(*line.font, line.text, start + line.width + SCROLL_GAP, line.baseline, line.color);
  }
  flushSurface();
}

static void setLine(TextLine &line, const char *text, const UiFont *font, int16_t baseline) {
  if (font == line.font && baseline == line.baseline && strcmp(text, line.text) == 0) return;
  strncpy(line.text, text, sizeof(line.text) - 1);
  line.text[sizeof(line.text) - 1] = '\0';
  line.font = font;
  line.baseline = baseline;
  line.width = textWidth(*font, line.text);
  line.offset = 0;
  line.scrolling = line.width > line.w;
  line.nextStepAt = millis() + SCROLL_PAUSE_MS;
  renderLine(line);
}

static void tickLine(TextLine &line, uint32_t now) {
  if (!line.scrolling || (int32_t)(now - line.nextStepAt) < 0) return;
  ++line.offset;
  if (line.offset >= line.width + SCROLL_GAP) {
    line.offset = 0;
    line.nextStepAt = now + SCROLL_PAUSE_MS;
  } else {
    line.nextStepAt = now + SCROLL_STEP_MS;
  }
  renderLine(line);
}

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------
static bool wifiConnected = false;
static uint8_t wifiBars = 0;
static uint8_t volumeLevel = 0;
static bool volumeMuted = false;
static bool haveTime = false;
static struct tm shownTime;
static char topKey[96] = "";
static char clockShown[6] = "";
static bool trackPaused = false;
static bool pausedShown = true;  // forces the first draw
static char bottomKey[128] = "";

static const char *const WEEKDAYS[] = {"Niedziela", "Poniedziałek", "Wtorek", "Środa",
                                       "Czwartek", "Piątek", "Sobota"};
static const char *const MONTHS[] = {"stycznia", "lutego", "marca", "kwietnia",
                                     "maja", "czerwca", "lipca", "sierpnia",
                                     "września", "października", "listopada", "grudnia"};

// ---------------------------------------------------------------------------
// Top bar: Wi-Fi, date, signal and volume
// ---------------------------------------------------------------------------
static void renderTopBar() {
  char key[sizeof(topKey)];
  snprintf(key, sizeof(key), "%d%d%u%d|%d-%d-%d-%d-%d", wifiConnected, wifiBars, volumeLevel,
           volumeMuted, haveTime, shownTime.tm_year, shownTime.tm_mon, shownTime.tm_mday,
           shownTime.tm_wday);
  if (strcmp(key, topKey) == 0) return;
  strcpy(topKey, key);

  beginSurface(0, 0, SCREEN_W, TOP_H);
  const uint16_t wifiColor = wifiConnected ? C[UI_C_ACCENT] : COL_DIM;
  surfDrawIcon(ICON_WIFI, MARGIN, 8, wifiColor);
  drawText(FONT_TOP, "WiFi", MARGIN + 28, TOP_BASELINE, wifiConnected ? C[UI_C_ACCENT] : COL_RED);

  if (haveTime) {
    char weekday[24], day[32], year[16];
    snprintf(weekday, sizeof(weekday), "%s, ", WEEKDAYS[shownTime.tm_wday % 7]);
    snprintf(day, sizeof(day), "%d %s", shownTime.tm_mday, MONTHS[shownTime.tm_mon % 12]);
    snprintf(year, sizeof(year), " %d", shownTime.tm_year + 1900);
    const int16_t total = textWidth(FONT_TOP, weekday) + textWidth(FONT_TOP, day) +
                          textWidth(FONT_TOP, year);
    int16_t x = (SCREEN_W - total) / 2;
    x = drawText(FONT_TOP, weekday, x, TOP_BASELINE, C[UI_C_DATE]);
    x = drawText(FONT_TOP, day, x, TOP_BASELINE, C[UI_C_DATE_ACCENT]);
    drawText(FONT_TOP, year, x, TOP_BASELINE, C[UI_C_DATE]);
  } else {
    const char *waiting = "Oczekiwanie na czas NTP";
    drawText(FONT_TOP, waiting, (SCREEN_W - textWidth(FONT_TOP, waiting)) / 2, TOP_BASELINE,
             C[UI_C_LABEL]);
  }

  // Volume (right aligned) and signal bars to its left.
  char volume[8];
  if (volumeMuted) strcpy(volume, "MUTE");
  else snprintf(volume, sizeof(volume), "%u", volumeLevel);
  const int16_t volumeWidth = textWidth(FONT_TOP, volume);
  const int16_t volumeX = SCREEN_W - MARGIN - volumeWidth;
  drawText(FONT_TOP, volume, volumeX, TOP_BASELINE, volumeMuted ? COL_RED : C[UI_C_VOLUME]);

  const int16_t barsRight = volumeX - (volumeWidth < 24 ? 24 - volumeWidth : 0) - 14;
  for (uint8_t i = 0; i < 4; ++i) {
    const int16_t barH = 5 + i * 4;
    const int16_t barX = barsRight - (3 - i) * 6 - 4;
    surfFillRect(barX, TOP_BASELINE - barH + 1, 4, barH,
                 (wifiConnected && i < wifiBars) ? C[UI_C_ACCENT] : COL_DIM);
  }
  flushSurface();
}

// ---------------------------------------------------------------------------
// Clock
// ---------------------------------------------------------------------------
static void renderClock(const char *text) {
  if (strcmp(text, clockShown) == 0) return;
  strncpy(clockShown, text, sizeof(clockShown) - 1);

  beginSurface(CLOCK_X, CLOCK_Y, CLOCK_W, CLOCK_H);
  // Fixed slot per digit (widest glyph + small gap) keeps the clock steady and compact.
  int16_t digitSlot = 0;
  for (char c = '0'; c <= '9'; ++c) {
    const UiGlyph *g = findGlyph(FONT_CLOCK, c);
    if (g && g->width > digitSlot) digitSlot = g->width;
  }
  digitSlot += CLOCK_DIGIT_GAP;
  const UiGlyph *zero = findGlyph(FONT_CLOCK, '0');
  const UiGlyph *colon = findGlyph(FONT_CLOCK, ':');
  const int16_t baseline = CLOCK_Y + CLOCK_BASELINE;
  const int16_t digitMiddle = baseline + zero->yOffset + zero->height / 2;
  const int16_t colonSlot = colon->width + 2 * CLOCK_COLON_GAP;
  int16_t x = CLOCK_X + (CLOCK_W - 4 * digitSlot - colonSlot) / 2;

  for (uint8_t i = 0; i < 5 && text[i]; ++i) {
    if (i == 2) {
      surfDrawMask(FONT_CLOCK.bitmap + colon->offset, colon->width, colon->height,
                   x + CLOCK_COLON_GAP, digitMiddle - colon->height / 2, C[UI_C_COLON]);
      x += colonSlot;
      continue;
    }
    const UiGlyph *g = findGlyph(FONT_CLOCK, text[i]);
    if (g) {
      const bool digit = text[i] >= '0' && text[i] <= '9';
      const int16_t top = digit ? baseline + g->yOffset : digitMiddle - g->height / 2;
      surfDrawMask(FONT_CLOCK.bitmap + g->offset, g->width, g->height,
                   x + (digitSlot - g->width) / 2, top, C[UI_C_CLOCK]);
    }
    x += digitSlot;
  }
  flushSurface();
}

// ---------------------------------------------------------------------------
// Labels, images, bottom bar
// ---------------------------------------------------------------------------
static void renderLabel(int16_t x, int16_t y, int16_t w, const char *text, uint16_t color) {
  beginSurface(x, y, w, 15);
  drawText(FONT_LABEL, text, x + (w - textWidth(FONT_LABEL, text, 1)) / 2, y + 12, color, 1);
  flushSurface();
}

// ---------------------------------------------------------------------------
// Spectrum analyzer
// ---------------------------------------------------------------------------
static uint8_t eqSegmentsShown[EQ_BANDS];
static uint8_t eqPeakShown[EQ_BANDS];
static bool eqDrawn = false;

// Classic hi-fi colours: green for the lower half, yellow above it, red at the top.
static uint16_t eqSegmentColor(uint8_t segment) {
  const uint8_t total = EQ_RANGE / EQ_SEGMENT_STEP;  // 11 segments
  if (segment >= total - 2) return C[UI_C_EQ_HIGH];
  if (segment >= total - 5) return C[UI_C_EQ_MID];
  return C[UI_C_EQ_LOW];
}

static void renderSpectrum(const uint8_t *segments, const uint8_t *peaks) {
  beginSurface(EQ_X, EQ_Y, EQ_W, EQ_H);
  const int16_t totalW = EQ_BANDS * EQ_BAR_W + (EQ_BANDS - 1) * EQ_BAR_GAP;
  const int16_t left = EQ_X + (EQ_W - totalW) / 2;
  surfFillRect(left, EQ_BOTTOM + 3, totalW, 1, C[UI_C_DIVIDER]);  // base line
  for (uint8_t b = 0; b < EQ_BANDS; ++b) {
    const int16_t x = left + b * (EQ_BAR_W + EQ_BAR_GAP);
    for (uint8_t s = 0; s < segments[b]; ++s) {
      surfFillRect(x, EQ_BOTTOM - s * EQ_SEGMENT_STEP - EQ_SEGMENT + 1, EQ_BAR_W, EQ_SEGMENT,
                   eqSegmentColor(s));
    }
    if (peaks[b] > 0) {  // thin marker one gap above the top of segment number peaks[b]
      const int16_t segmentTop = EQ_BOTTOM - (peaks[b] - 1) * EQ_SEGMENT_STEP - EQ_SEGMENT + 1;
      surfFillRect(x, segmentTop - 3, EQ_BAR_W, 2, eqSegmentColor(peaks[b] - 1));
    }
  }
  flushSurface();
}

// ---------------------------------------------------------------------------
// Weather
// ---------------------------------------------------------------------------
static char weatherKey[64] = "";
static const uint8_t WX_KINDS = sizeof(WEATHER_ICONS) / sizeof(WEATHER_ICONS[0]);
static const uint16_t *customWeatherIcons[WX_KINDS] = {nullptr};

const uint16_t *uiWeatherIcon(uint8_t kind) {
  if (kind >= WX_KINDS) kind = UI_WX_CLOUDY;
  return customWeatherIcons[kind] ? customWeatherIcons[kind] : WEATHER_ICONS[kind];
}

bool uiWeatherIconIsOwn(uint8_t kind) { return kind < WX_KINDS && customWeatherIcons[kind] != nullptr; }

static void surfDrawImage(const uint16_t *image, int16_t w, int16_t h, int16_t x, int16_t y) {
  for (int16_t row = 0; row < h; ++row) {
    const int16_t yy = y - surfY + row;
    if (yy < 0 || yy >= surfH) continue;
    for (int16_t col = 0; col < w; ++col) {
      const int16_t xx = x - surfX + col;
      if (xx >= 0 && xx < surfW) surface[(uint32_t)yy * surfW + xx] = image[(uint32_t)row * w + col];
    }
  }
}

static void renderWeather(bool valid, uint8_t kind, int16_t tempTenths, const char *place) {
  char key[sizeof(weatherKey)];
  snprintf(key, sizeof(key), "%d|%u|%d|%.40s", valid, kind, tempTenths, place ? place : "");
  if (strcmp(key, weatherKey) == 0) return;
  strcpy(weatherKey, key);

  // Left of the clock: temperature and place.
  beginSurface(WX_TEXT_X, WX_Y, WX_TEXT_W, WX_H);
  if (valid) {
    char temp[16];
    const int16_t rounded = (tempTenths >= 0 ? tempTenths + 5 : tempTenths - 5) / 10;
    snprintf(temp, sizeof(temp), "%d\xC2\xB0", rounded);
    drawText(FONT_TEMP, temp, WX_TEXT_X + (WX_TEXT_W - textWidth(FONT_TEMP, temp)) / 2, WX_Y + 43,
             C[UI_C_TEMP]);
    // Place name: 17 px medium (readable from a distance), smaller font when it is long.
    const char *name = place ? place : "";
    const UiFont &font = textWidth(FONT_TOP, name) <= WX_TEXT_W ? FONT_TOP : FONT_SMALL;
    char fitted[48];
    ellipsize(font, name, WX_TEXT_W, fitted, sizeof(fitted));
    drawText(font, fitted, WX_TEXT_X + (WX_TEXT_W - textWidth(font, fitted)) / 2, WX_Y + 65,
             C[UI_C_CITY]);
  }
  flushSurface();

  // Right of the clock: the icon as large as the space allows.
  beginSurface(WX_ICON_X, WX_Y, WX_ICON_AREA_W, WX_H);
  if (valid) {
    surfDrawImage(uiWeatherIcon(kind), WX_ICON_W, WX_ICON_H, WX_ICON_X + (WX_ICON_AREA_W - WX_ICON_W) / 2,
                  WX_Y + (WX_H - WX_ICON_H) / 2);
  }
  flushSurface();
}

static void drawImage(int16_t x, int16_t y, const uint16_t *image, int16_t w, int16_t h) {
  if (overlayMode != OVERLAY_NONE) return;  // main screen hidden
  gfx->draw16bitRGBBitmap(x, y, (uint16_t *)image, w, h);
  shadowBlit(x, y, image, w, h);
}

// ---------------------------------------------------------------------------
// Rounded frames around every block of the main screen.
// No region overlaps a frame line, so they are drawn once (and on colour change).
// ---------------------------------------------------------------------------
struct UiFrame {
  int16_t x0, y0, x1, y1;  // inclusive line coordinates
};
static const UiFrame FRAMES[] = {
    {9, 42, 106, 118},    // temperature and place
    {112, 42, 366, 118},  // clock
    {372, 42, 470, 118},  // weather icon
    {9, 122, 106, 203},   // station logo
    {112, 122, 470, 203}, // station name and description
    {9, 205, 106, 279},   // vinyl record
    {112, 205, 320, 279}, // now playing
    {326, 205, 470, 279}, // spectrum analyzer
};
static const int16_t FRAME_RADIUS = 10;

static uint16_t mix565(uint16_t bg, uint16_t fg, uint8_t alpha) {  // alpha 0..255
  const int rb = (bg >> 11) & 0x1F, gb = (bg >> 5) & 0x3F, bb = bg & 0x1F;
  const int rf = (fg >> 11) & 0x1F, gf = (fg >> 5) & 0x3F, bf = fg & 0x1F;
  const int r = rb + (rf - rb) * alpha / 255;
  const int g = gb + (gf - gb) * alpha / 255;
  const int b = bb + (bf - bb) * alpha / 255;
  return (uint16_t)((r << 11) | (g << 5) | b);
}

static void frameLine(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t c) {
  if (w <= 0 || h <= 0) return;
  gfx->fillRect(x, y, w, h, c);
  shadowFill(x, y, w, h, c);
}

static void drawFrame(const UiFrame &f, uint16_t color) {
  const int16_t r = FRAME_RADIUS;
  frameLine(f.x0 + r + 1, f.y0, f.x1 - f.x0 - 2 * r - 1, 1, color);
  frameLine(f.x0 + r + 1, f.y1, f.x1 - f.x0 - 2 * r - 1, 1, color);
  frameLine(f.x0, f.y0 + r + 1, 1, f.y1 - f.y0 - 2 * r - 1, color);
  frameLine(f.x1, f.y0 + r + 1, 1, f.y1 - f.y0 - 2 * r - 1, color);
  // Antialiased corners: signed distance to the rounded rectangle through the line centres.
  const float cx = (f.x0 + f.x1) * 0.5f + 0.5f, cy = (f.y0 + f.y1) * 0.5f + 0.5f;
  const float hx = (f.x1 - f.x0) * 0.5f - r, hy = (f.y1 - f.y0) * 0.5f - r;
  const int16_t xs[2] = {(int16_t)(f.x0 - 1), (int16_t)(f.x1 - r)};
  const int16_t ys[2] = {(int16_t)(f.y0 - 1), (int16_t)(f.y1 - r)};
  for (uint8_t cyI = 0; cyI < 2; ++cyI) {
    for (uint8_t cxI = 0; cxI < 2; ++cxI) {
      for (int16_t y = ys[cyI]; y <= ys[cyI] + r + 1; ++y) {
        for (int16_t x = xs[cxI]; x <= xs[cxI] + r + 1; ++x) {
          const float qx = fabsf(x + 0.5f - cx) - hx, qy = fabsf(y + 0.5f - cy) - hy;
          const float ox = qx > 0 ? qx : 0, oy = qy > 0 ? qy : 0;
          const float inside = qx > qy ? qx : qy;
          const float d = sqrtf(ox * ox + oy * oy) + (inside < 0 ? inside : 0) - r;
          const float cover = 1.0f - fabsf(d);
          if (cover <= 0) continue;
          const uint16_t c = mix565(COL_BG, color, (uint8_t)(cover * 255 + 0.5f));
          gfx->drawPixel(x, y, c);
          shadowFill(x, y, 1, 1, c);
        }
      }
    }
  }
}

static void drawFrames() {
  for (const UiFrame &f : FRAMES) drawFrame(f, C[UI_C_FRAME]);
}

// ---------------------------------------------------------------------------
// Spinning vinyl record in the cover area while audio is playing.
// The static picture is built once (and on colour change); each frame only adds
// the rotating sheen and the label dot.
// ---------------------------------------------------------------------------
static const int16_t VINYL_CX = COVER_W / 2, VINYL_CY = COVER_H / 2, VINYL_R = 30, VINYL_LABEL_R = 11;
static const uint32_t VINYL_FRAME_MS = 66;  // ~15 frames per second
static const uint8_t VINYL_STEP = 8;        // 1/32 turn per frame, about 2 s per turn
static const uint32_t VINYL_AUDIO_HOLD_MS = 1500;
static uint16_t *vinylBase = nullptr;   // COVER_W x COVER_H RGB565
static uint16_t *vinylAngle = nullptr;  // bit 8 = sheen ring, bits 0..7 = angle
static uint8_t vinylPhase = 0;
static uint32_t vinylNextAt = 0, lastAudioAt = 0;

static void vinylSample(float x, float y, float &r, float &g, float &b) {
  auto set = [&](uint8_t rr, uint8_t gg, uint8_t bb) { r += rr; g += gg; b += bb; };
  const float dx = x - VINYL_CX, dy = y - VINYL_CY, rad = sqrtf(dx * dx + dy * dy);
  if (rad <= 2) return set(10, 10, 10);
  if (rad <= VINYL_LABEL_R) {
    const uint16_t a = C[UI_C_ACCENT];
    return set(((a >> 11) & 0x1F) * 255 / 31, ((a >> 5) & 0x3F) * 255 / 63, (a & 0x1F) * 255 / 31);
  }
  if (rad <= VINYL_R - 1) {
    for (int gr = 13; gr <= 28; gr += 3) {
      if (fabsf(rad - gr) < 0.25f) return set(30, 30, 34);
    }
    return set(14, 14, 16);
  }
  if (rad <= VINYL_R) set(60, 60, 66);  // rim; black outside
}

static void buildVinyl() {
  const size_t n = (size_t)COVER_W * COVER_H;
#ifdef ARDUINO
  if (vinylBase == nullptr) vinylBase = (uint16_t *)heap_caps_malloc(n * 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (vinylAngle == nullptr) vinylAngle = (uint16_t *)heap_caps_malloc(n * 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#endif
  if (vinylBase == nullptr) vinylBase = (uint16_t *)malloc(n * 2);
  if (vinylAngle == nullptr) vinylAngle = (uint16_t *)malloc(n * 2);
  if (vinylBase == nullptr || vinylAngle == nullptr) return;
  for (int16_t y = 0; y < COVER_H; ++y) {
    for (int16_t x = 0; x < COVER_W; ++x) {
      float r = 0, g = 0, b = 0;
      for (uint8_t sy = 0; sy < 4; ++sy) {
        for (uint8_t sx = 0; sx < 4; ++sx) vinylSample(x + (sx + 0.5f) / 4, y + (sy + 0.5f) / 4, r, g, b);
      }
      const size_t i = (size_t)y * COVER_W + x;
      vinylBase[i] = rgb((uint8_t)(r / 16), (uint8_t)(g / 16), (uint8_t)(b / 16));
      const float dx = x + 0.5f - VINYL_CX, dy = y + 0.5f - VINYL_CY, rad = sqrtf(dx * dx + dy * dy);
      const uint8_t angle = (uint8_t)((int)(atan2f(dy, dx) * (256.0f / 6.2831853f)) & 0xFF);
      vinylAngle[i] = (rad > VINYL_LABEL_R + 0.5f && rad < VINYL_R * 0.93f) ? (uint16_t)(0x100 | angle) : angle;
    }
  }
}

static void renderVinyl() {
  if (vinylBase == nullptr || vinylAngle == nullptr) {
    drawImage(COVER_X, COVER_Y, COVER_PLACEHOLDER, COVER_W, COVER_H);
    return;
  }
  beginSurface(COVER_X, COVER_Y, COVER_W, COVER_H);
  static const uint16_t SHEEN = rgb(170, 180, 200);
  const size_t n = (size_t)COVER_W * COVER_H;
  for (size_t i = 0; i < n; ++i) {
    uint16_t p = vinylBase[i];
    const uint16_t v = vinylAngle[i];
    if (v & 0x100) {  // two opposite light wedges
      const uint8_t d = (uint8_t)((uint8_t)v - vinylPhase) & 127;
      const uint8_t dist = d < 64 ? d : 128 - d;
      if (dist < 16) p = mix565(p, SHEEN, dist < 10 ? 55 : (uint8_t)(55 * (16 - dist) / 6));
    }
    surface[i] = p;
  }
  // White dot on the label shows the rotation.
  const float a = vinylPhase * (6.2831853f / 256.0f);
  const float dotX = VINYL_CX + 6 * cosf(a), dotY = VINYL_CY + 6 * sinf(a);
  for (int16_t y = (int16_t)dotY - 3; y <= (int16_t)dotY + 3; ++y) {
    for (int16_t x = (int16_t)dotX - 3; x <= (int16_t)dotX + 3; ++x) {
      if (x < 0 || y < 0 || x >= COVER_W || y >= COVER_H) continue;
      const float dx = x + 0.5f - dotX, dy = y + 0.5f - dotY;
      const float cover = 2.5f - sqrtf(dx * dx + dy * dy);
      if (cover <= 0) continue;
      uint16_t &dst = surface[(size_t)y * COVER_W + x];
      dst = mix565(dst, rgb(230, 240, 255), cover >= 1 ? 255 : (uint8_t)(cover * 255));
    }
  }
  flushSurface();
}

static void tickVinyl(uint32_t now) {
  const bool playing = !trackPaused && lastAudioAt != 0 && now - lastAudioAt < VINYL_AUDIO_HOLD_MS;
  if (!playing || (int32_t)(now - vinylNextAt) < 0) return;
  vinylNextAt = now + VINYL_FRAME_MS;
  vinylPhase += VINYL_STEP;
  renderVinyl();
}

// ---------------------------------------------------------------------------
// Temporary screens: station list and volume (opened by the encoders)
// ---------------------------------------------------------------------------
static const uint32_t OVERLAY_TIMEOUT_MS = 2000;  // back to the main screen after 2 s idle
static bool overlayDirty = false;
static uint32_t overlayActivityAt = 0;
static uint16_t listCount = 0, listSelected = 0;
static UiStationName listNameAt = nullptr;
static uint8_t overlayVolume = 0, overlayVolumeMax = 21;
static bool overlayMuted = false;

static const int16_t LIST_ROWS = 5;  // rows that fit between the header and the hint
static const int16_t LIST_TOP = 50, LIST_ROW_H = 45;
static const int16_t PANEL_X0 = 9, PANEL_Y0 = 42, PANEL_X1 = 470, PANEL_Y1 = 279;  // like the frames

// Antialiased rounded rectangle on the surface; the outline runs through the centres of the
// x0..x1 / y0..y1 pixels (same geometry as the frames of the main screen).
static void surfRoundRect(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t r, bool filled,
                          uint16_t fill, bool outlined, uint16_t line) {
  const float cx = (x0 + x1) * 0.5f + 0.5f, cy = (y0 + y1) * 0.5f + 0.5f;
  const float hx = (x1 - x0) * 0.5f - r, hy = (y1 - y0) * 0.5f - r;
  const int16_t sx0 = x0 - 1 > surfX ? x0 - 1 : surfX;
  const int16_t sx1 = x1 + 1 < surfX + surfW - 1 ? x1 + 1 : surfX + surfW - 1;
  const int16_t sy0 = y0 - 1 > surfY ? y0 - 1 : surfY;
  const int16_t sy1 = y1 + 1 < surfY + surfH - 1 ? y1 + 1 : surfY + surfH - 1;
  for (int16_t y = sy0; y <= sy1; ++y) {
    const float qy = fabsf(y + 0.5f - cy) - hy;
    for (int16_t x = sx0; x <= sx1; ++x) {
      const float qx = fabsf(x + 0.5f - cx) - hx;
      float d;
      if (qx <= 0 && qy <= 0) {
        d = (qx > qy ? qx : qy) - r;
        if (!filled && d < -1) continue;  // well inside an outline-only box
      } else {
        const float ox = qx > 0 ? qx : 0, oy = qy > 0 ? qy : 0;
        d = sqrtf(ox * ox + oy * oy) + fminf(fmaxf(qx, qy), 0) - r;
      }
      uint16_t &dst = surface[(uint32_t)(y - surfY) * surfW + (x - surfX)];
      if (filled) {
        const float c = 0.5f - d;
        if (c > 0) dst = mix565(dst, fill, c >= 1 ? 255 : (uint8_t)(c * 255));
      }
      if (outlined) {
        const float c = 1.0f - fabsf(d);
        if (c > 0) dst = mix565(dst, line, (uint8_t)(c * 255));
      }
    }
  }
}

static void drawCentered(const UiFont &font, const char *text, int16_t baseline, uint16_t color) {
  drawText(font, text, (SCREEN_W - textWidth(font, text)) / 2, baseline, color);
}

static void drawStationList() {
  drawText(FONT_TOP, "Wybór stacji", 20, 27, C[UI_C_ACCENT]);
  char position[16];
  snprintf(position, sizeof(position), "%u / %u", (unsigned)(listCount ? listSelected + 1 : 0),
           (unsigned)listCount);
  drawText(FONT_TOP, position, 460 - textWidth(FONT_TOP, position), 27, C[UI_C_BOTTOM]);
  surfRoundRect(PANEL_X0, PANEL_Y0, PANEL_X1, PANEL_Y1, FRAME_RADIUS, false, 0, true, C[UI_C_FRAME]);
  drawCentered(FONT_SMALL, "Obracaj pokrętłem, aby zmienić stację", 306, C[UI_C_DESC]);
  if (listCount == 0 || listNameAt == nullptr) {
    drawCentered(FONT_ARTIST, "Brak stacji", 168, C[UI_C_ARTIST]);
    return;
  }
  // The selected station stays in the middle row when possible.
  const bool scrollbar = listCount > LIST_ROWS;
  int16_t top = (int16_t)listSelected - LIST_ROWS / 2;
  if (top > (int16_t)listCount - LIST_ROWS) top = (int16_t)listCount - LIST_ROWS;
  if (top < 0) top = 0;
  const int16_t rowRight = scrollbar ? 450 : 462;
  for (int16_t row = 0; row < LIST_ROWS && top + row < (int16_t)listCount; ++row) {
    const uint16_t index = (uint16_t)(top + row);
    const bool selected = index == listSelected;
    const int16_t y = LIST_TOP + row * LIST_ROW_H;
    if (y > surfY + surfH || y + LIST_ROW_H < surfY) continue;  // not in this strip
    if (selected) {
      surfRoundRect(16, y, rowRight, y + LIST_ROW_H - 4, 8, true, mix565(COL_BG, C[UI_C_ACCENT], 70),
                    true, C[UI_C_ACCENT]);
    }
    const int16_t middle = y + (LIST_ROW_H - 4) / 2;
    char number[8];
    snprintf(number, sizeof(number), "%u", (unsigned)index + 1);
    drawText(FONT_TOP, number, 56 - textWidth(FONT_TOP, number), middle + FONT_TOP.capHeight / 2,
             selected ? C[UI_C_ARTIST] : C[UI_C_DESC]);
    char fitted[96];
    const char *name = listNameAt(index);
    ellipsize(FONT_ARTIST, name ? name : "", rowRight - 10 - 70, fitted, sizeof(fitted));
    drawText(FONT_ARTIST, fitted, 70, middle + FONT_ARTIST.capHeight / 2,
             selected ? C[UI_C_ARTIST] : C[UI_C_CITY]);
  }
  if (scrollbar) {
    const int16_t trackY = LIST_TOP + 2, trackH = LIST_ROWS * LIST_ROW_H - 8;
    int16_t thumbH = (int16_t)((int32_t)trackH * LIST_ROWS / listCount);
    if (thumbH < 16) thumbH = 16;
    const int16_t thumbY = trackY + (int16_t)((int32_t)(trackH - thumbH) * top / (listCount - LIST_ROWS));
    surfRoundRect(457, trackY, 460, trackY + trackH - 1, 1, true, C[UI_C_DIVIDER], false, 0);
    surfRoundRect(457, thumbY, 460, thumbY + thumbH - 1, 1, true, C[UI_C_ACCENT], false, 0);
  }
}

static void drawVolumeScreen() {
  drawCentered(FONT_TOP, "Głośność", 27, C[UI_C_ACCENT]);
  surfRoundRect(PANEL_X0, PANEL_Y0, PANEL_X1, PANEL_Y1, FRAME_RADIUS, false, 0, true, C[UI_C_FRAME]);
  char number[6];
  snprintf(number, sizeof(number), "%u", (unsigned)overlayVolume);
  drawCentered(FONT_CLOCK, number, 140, overlayMuted ? C[UI_C_DESC] : C[UI_C_CLOCK]);
  if (overlayMuted) drawCentered(FONT_TOP, "WYCISZONE", 171, COL_RED);

  // Scale: one bar per volume step, growing to the right.
  const int16_t steps = overlayVolumeMax ? overlayVolumeMax : 1;
  int16_t gap = 6;
  int16_t barW = (392 - (steps - 1) * gap) / steps;
  if (barW < 4) {
    gap = 2;
    barW = (392 - (steps - 1) * gap) / steps;
    if (barW < 1) barW = 1;
  }
  const int16_t totalW = steps * barW + (steps - 1) * gap;
  const int16_t left = (SCREEN_W - totalW) / 2, bottom = 252;
  const uint16_t on = overlayMuted ? C[UI_C_DESC] : C[UI_C_ACCENT];
  for (int16_t i = 0; i < steps; ++i) {
    const int16_t h = 10 + (steps > 1 ? 60 * i / (steps - 1) : 60);
    const int16_t x = left + i * (barW + gap);
    surfRoundRect(x, bottom - h + 1, x + barW - 1, bottom, barW >= 8 ? 3 : 0, true,
                  i < overlayVolume ? on : C[UI_C_DIVIDER], false, 0);
  }
  char maxText[6];
  snprintf(maxText, sizeof(maxText), "%u", (unsigned)overlayVolumeMax);
  drawText(FONT_SMALL, "0", left, 272, C[UI_C_DESC]);
  drawText(FONT_SMALL, maxText, left + totalW - textWidth(FONT_SMALL, maxText), 272, C[UI_C_DESC]);
  drawCentered(FONT_SMALL, "Obracaj pokrętłem, aby zmienić głośność", 306, C[UI_C_DESC]);
}

// Wi-Fi setup screen: shown while the setup access point is the only way to reach the radio.
static bool setupActive = false;
static char setupSsid[40] = "", setupPassword[72] = "", setupAddress[40] = "", setupNote[96] = "";

// Largest font in which the text fits maxWidth (the smallest one is cut with "…").
static void drawFitted(const UiFont *const *fonts, uint8_t count, const char *text, int16_t baseline,
                       int16_t maxWidth, uint16_t color) {
  for (uint8_t i = 0; i < count; ++i) {
    if (textWidth(*fonts[i], text) <= maxWidth || i + 1 == count) {
      char fitted[96];
      ellipsize(*fonts[i], text, maxWidth, fitted, sizeof(fitted));
      drawCentered(*fonts[i], fitted, baseline, color);
      return;
    }
  }
}

static void drawSetupScreen() {
  drawCentered(FONT_TOP, "Konfiguracja Wi-Fi", 27, C[UI_C_ACCENT]);
  surfRoundRect(PANEL_X0, PANEL_Y0, PANEL_X1, PANEL_Y1, FRAME_RADIUS, false, 0, true, C[UI_C_FRAME]);
  const int16_t maxW = PANEL_X1 - PANEL_X0 - 32;
  static const UiFont *const BIG[] = {&FONT_STATION, &FONT_STATION_SMALL, &FONT_ARTIST};
  static const UiFont *const MID[] = {&FONT_ARTIST, &FONT_TITLE, &FONT_SMALL};

  drawCentered(FONT_SMALL, "1. Połącz telefon lub komputer z siecią Wi-Fi:", 70, C[UI_C_DESC]);
  drawFitted(BIG, 3, setupSsid, 112, maxW, C[UI_C_STATION]);
  char line[96];
  if (setupPassword[0]) snprintf(line, sizeof(line), "Hasło:  %s", setupPassword);
  else strcpy(line, "Sieć bez hasła");
  drawFitted(MID, 3, line, 148, maxW, C[UI_C_ARTIST]);

  drawCentered(FONT_SMALL, "2. Otwórz w przeglądarce (zwykle otworzy się sama):", 190, C[UI_C_DESC]);
  drawFitted(MID, 3, setupAddress, 226, maxW, C[UI_C_TITLE]);
  drawCentered(FONT_SMALL, "3. W Ustawieniach > Wi-Fi wybierz swoją sieć i zapisz", 262, C[UI_C_DESC]);

  if (setupNote[0]) {
    char fitted[96];
    ellipsize(FONT_SMALL, setupNote, SCREEN_W - 24, fitted, sizeof(fitted));
    drawCentered(FONT_SMALL, fitted, 306, C[UI_C_BOTTOM]);
  }
}

static void renderOverlay() {
  for (int16_t y = 0; y < SCREEN_H; y += 80) {  // the surface holds 480x80 pixels
    beginSurface(0, y, SCREEN_W, 80);
    if (overlayMode == OVERLAY_STATIONS) drawStationList();
    else if (overlayMode == OVERLAY_SETUP) drawSetupScreen();
    else drawVolumeScreen();
    pushSurface();
  }
}

static void openOverlay(uint8_t mode) {
  overlayMode = mode;
  overlayDirty = true;
  overlayActivityAt = millis();
}

struct BottomCell {
  int16_t left, right;
};
static const BottomCell BOTTOM_CELLS[5] = {{12, 138}, {138, 214}, {214, 308}, {308, 396}, {396, 468}};

static void drawBottomCell(uint8_t index, const UiMask &icon, const char *text) {
  const BottomCell &cell = BOTTOM_CELLS[index];
  const int16_t inner = cell.right - cell.left - 6;
  const int16_t gap = 6;
  char fitted[48];
  ellipsize(FONT_SMALL, text, inner - icon.width - gap, fitted, sizeof(fitted));
  const int16_t contentW = icon.width + gap + textWidth(FONT_SMALL, fitted);
  const int16_t x = cell.left + (cell.right - cell.left - contentW) / 2;
  const int16_t baseline = BOTTOM_Y + BOTTOM_BASELINE;
  const int16_t capHeight = FONT_SMALL.capHeight;
  surfDrawIcon(icon, x, baseline - capHeight / 2 - icon.height / 2, C[UI_C_BOTTOM]);
  drawText(FONT_SMALL, fitted, x + icon.width + gap, baseline, C[UI_C_BOTTOM]);
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
static void applyColors(const uint32_t *rgb) {
  for (uint8_t i = 0; i < UI_C_COUNT; ++i) C[i] = rgb888(rgb[i]);
  stationLine.color = C[UI_C_STATION];
  descriptionLine.color = C[UI_C_DESC];
  artistLine.color = C[UI_C_ARTIST];
  titleLine.color = C[UI_C_TITLE];
}

static void drawDividers() {
  const uint16_t c = C[UI_C_DIVIDER];
  gfx->drawFastHLine(MARGIN, TOP_DIVIDER_Y, SCREEN_W - 2 * MARGIN, c);
  shadowFill(MARGIN, TOP_DIVIDER_Y, SCREEN_W - 2 * MARGIN, 1, c);
  // No line above the bottom bar: the frames end right above it.
  for (uint8_t i = 1; i < 5; ++i) {
    gfx->drawFastVLine(BOTTOM_CELLS[i].left, BOTTOM_Y + 6, BOTTOM_H - 12, c);
    shadowFill(BOTTOM_CELLS[i].left, BOTTOM_Y + 6, 1, BOTTOM_H - 12, c);
  }
}

bool uiBegin(Arduino_GFX *display) {
  gfx = display;
  applyColors(UI_DEFAULT_COLORS);
#ifdef ARDUINO
  surface = (uint16_t *)heap_caps_malloc(SURFACE_PIXELS * 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#endif
  if (surface == nullptr) surface = (uint16_t *)malloc(SURFACE_PIXELS * 2);
  if (surface == nullptr) return false;

#ifdef ARDUINO
  shadow = (uint16_t *)heap_caps_malloc((size_t)SCREEN_W * SCREEN_H * 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#endif  // no PSRAM: the radio works, only the web preview is unavailable
  gfx->fillScreen(COL_BG);
  shadowFill(0, 0, SCREEN_W, SCREEN_H, COL_BG);
  drawDividers();
  drawFrames();
  renderLabel(STATION_TEXT_X, 126, STATION_TEXT_W, "STACJA RADIOWA", C[UI_C_LABEL]);
  buildVinyl();
  renderVinyl();
  uiSetPaused(false);
  uiSetTime(nullptr);
  renderClock("--:--");
  uiSetSpectrum(nullptr, nullptr);
  return true;
}

void uiSetWifi(bool connected, uint8_t signalBars) {
  wifiConnected = connected;
  wifiBars = signalBars > 4 ? 4 : signalBars;
  renderTopBar();
}

void uiSetVolume(uint8_t volume, bool muted) {
  volumeLevel = volume;
  volumeMuted = muted;
  if (overlayMode == OVERLAY_VOLUME && (volume != overlayVolume || muted != overlayMuted)) {
    overlayVolume = volume;  // e.g. changed in the web panel while the volume screen is open
    overlayMuted = muted;
    overlayDirty = true;
  }
  renderTopBar();
}

void uiSetTime(const struct tm *localTime) {
  if (localTime == nullptr) {
    haveTime = false;
    memset(&shownTime, 0, sizeof(shownTime));
    renderTopBar();
    renderClock("--:--");
    return;
  }
  haveTime = true;
  shownTime = *localTime;
  renderTopBar();
  char text[8];
  snprintf(text, sizeof(text), "%02u:%02u", (unsigned)(localTime->tm_hour % 24),
           (unsigned)(localTime->tm_min % 60));
  renderClock(text);
}

static const uint16_t *shownLogo = nullptr;

void uiSetLogo(const uint16_t *logo) {
  // Always redrawn: the caller may reuse one buffer for different logos.
  shownLogo = logo ? logo : LOGO_PLACEHOLDER;
  drawImage(LOGO_X, LOGO_Y, shownLogo, LOGO_W, LOGO_H);
}

void uiSetStationName(const char *name) {
  if (textWidth(FONT_STATION, name) <= stationLine.w) {
    setLine(stationLine, name, &FONT_STATION, STATION_BASELINE_LARGE);
  } else {
    setLine(stationLine, name, &FONT_STATION_SMALL, STATION_BASELINE_SMALL);
  }
}

void uiSetDescription(const char *text) {
  setLine(descriptionLine, text ? text : "", &FONT_SMALL, descriptionLine.baseline);
}

void uiSetTrack(const char *artist, const char *title) {
  setLine(artistLine, artist, &FONT_ARTIST, artistLine.baseline);
  setLine(titleLine, title, &FONT_TITLE, titleLine.baseline);
}

void uiSetPaused(bool paused) {
  trackPaused = paused;
  if (paused == pausedShown) return;
  pausedShown = paused;
  renderLabel(TRACK_TEXT_X, 207, TRACK_TEXT_W, paused ? "PAUZA" : "AKTUALNIE GRA:",
              paused ? C[UI_C_ACCENT] : C[UI_C_LABEL]);
}

static struct {
  char source[32];
  uint8_t number, count;
  uint16_t kbps;
  char genre[48];
  int16_t offset;
  bool set;
} lastBottom = {"", 0, 0, 0, "", 0, false};
static struct {
  bool valid;
  uint8_t kind;
  int16_t temp;
  char place[48];
  bool set;
} lastWeather = {false, 0, 0, "", false};

void uiSetBottomInfo(const char *source, uint8_t stationNumber, uint8_t stationCount,
                     uint16_t kbps, const char *genre, int16_t utcOffsetMinutes) {
  strncpy(lastBottom.source, source ? source : "", sizeof(lastBottom.source) - 1);
  lastBottom.number = stationNumber;
  lastBottom.count = stationCount;
  lastBottom.kbps = kbps;
  strncpy(lastBottom.genre, genre ? genre : "", sizeof(lastBottom.genre) - 1);
  lastBottom.offset = utcOffsetMinutes;
  lastBottom.set = true;
  char stations[12], bitrate[16], zone[16];
  snprintf(stations, sizeof(stations), "%u/%u", stationNumber, stationCount);
  if (kbps) snprintf(bitrate, sizeof(bitrate), "%u kbps", kbps);
  else strcpy(bitrate, "-- kbps");
  const int16_t absMinutes = utcOffsetMinutes < 0 ? -utcOffsetMinutes : utcOffsetMinutes;
  if (absMinutes % 60) {
    snprintf(zone, sizeof(zone), "UTC%c%d:%02d", utcOffsetMinutes < 0 ? '-' : '+',
             absMinutes / 60, absMinutes % 60);
  } else {
    snprintf(zone, sizeof(zone), "UTC%c%d", utcOffsetMinutes < 0 ? '-' : '+', absMinutes / 60);
  }
  const char *genreText = (genre && genre[0]) ? genre : "\xE2\x80\x94";  // em dash

  char key[sizeof(bottomKey)];
  snprintf(key, sizeof(key), "%.30s|%s|%s|%.40s|%s", source, stations, bitrate, genreText, zone);
  if (strcmp(key, bottomKey) == 0) return;
  strcpy(bottomKey, key);

  // Each cell is redrawn as its own region so the separators stay untouched.
  const UiMask *icons[5] = {&ICON_GLOBE, &ICON_HEART, &ICON_BITRATE, &ICON_GENRE, &ICON_TIMEZONE};
  const char *texts[5] = {source, stations, bitrate, genreText, zone};
  for (uint8_t i = 0; i < 5; ++i) {
    const BottomCell &cell = BOTTOM_CELLS[i];
    beginSurface(cell.left + 1, BOTTOM_Y, cell.right - cell.left - 2, BOTTOM_H);
    drawBottomCell(i, *icons[i], texts[i]);
    flushSurface();
  }
}

void uiSetSpectrum(const uint8_t *bars, const uint8_t *peaks) {
  uint8_t segments[EQ_BANDS];
  uint8_t peakSegments[EQ_BANDS];
  const uint8_t maxSegments = EQ_RANGE / EQ_SEGMENT_STEP;
  bool changed = !eqDrawn;
  for (uint8_t b = 0; b < EQ_BANDS; ++b) {
    const uint8_t bar = bars ? bars[b] : 0;
    const uint8_t peak = peaks ? peaks[b] : 0;
    if (bar) lastAudioAt = millis() | 1;
    segments[b] = (uint8_t)((bar * maxSegments + 127) / 255);
    peakSegments[b] = (uint8_t)((peak * maxSegments + 127) / 255);
    if (peakSegments[b] < segments[b]) peakSegments[b] = segments[b];
    changed = changed || segments[b] != eqSegmentsShown[b] || peakSegments[b] != eqPeakShown[b];
  }
  if (!changed) return;
  memcpy(eqSegmentsShown, segments, sizeof(segments));
  memcpy(eqPeakShown, peakSegments, sizeof(peakSegments));
  eqDrawn = true;
  renderSpectrum(segments, peakSegments);
}

void uiSetWeather(bool valid, uint8_t kind, int16_t temperatureTenths, const char *place) {
  lastWeather.valid = valid;
  lastWeather.kind = kind;
  lastWeather.temp = temperatureTenths;
  strncpy(lastWeather.place, place ? place : "", sizeof(lastWeather.place) - 1);
  lastWeather.set = true;
  renderWeather(valid, kind, temperatureTenths, place);
}

// Draws the whole main screen again from the remembered state.
static void redrawMain(bool clear) {
  if (clear) {
    gfx->fillScreen(COL_BG);
    shadowFill(0, 0, SCREEN_W, SCREEN_H, COL_BG);
    if (shownLogo) drawImage(LOGO_X, LOGO_Y, shownLogo, LOGO_W, LOGO_H);
  }
  drawDividers();
  drawFrames();
  renderVinyl();
  renderLabel(STATION_TEXT_X, 126, STATION_TEXT_W, "STACJA RADIOWA", C[UI_C_LABEL]);
  pausedShown = !trackPaused;
  uiSetPaused(trackPaused);
  topKey[0] = '\0';
  renderTopBar();
  char clock[sizeof(clockShown)];
  strcpy(clock, clockShown);
  clockShown[0] = '\0';
  renderClock(clock[0] ? clock : "--:--");
  renderLine(stationLine);
  renderLine(descriptionLine);
  renderLine(artistLine);
  renderLine(titleLine);
  if (lastWeather.set) {
    weatherKey[0] = '\0';
    renderWeather(lastWeather.valid, lastWeather.kind, lastWeather.temp, lastWeather.place);
  }
  if (lastBottom.set) {
    bottomKey[0] = '\0';
    char source[32], genre[48];
    strcpy(source, lastBottom.source);
    strcpy(genre, lastBottom.genre);
    uiSetBottomInfo(source, lastBottom.number, lastBottom.count, lastBottom.kbps, genre,
                    lastBottom.offset);
  }
  if (eqDrawn) renderSpectrum(eqSegmentsShown, eqPeakShown);
}

void uiSetWeatherIcon(uint8_t kind, const uint16_t *pixels) {
  if (kind >= WX_KINDS) return;
  customWeatherIcons[kind] = pixels;
  if (lastWeather.set && lastWeather.valid && lastWeather.kind == kind) {
    weatherKey[0] = '\0';  // the icon on the screen changed
    renderWeather(lastWeather.valid, lastWeather.kind, lastWeather.temp, lastWeather.place);
  }
}

void uiSetColors(const uint32_t *rgb) {
  applyColors(rgb ? rgb : UI_DEFAULT_COLORS);
  buildVinyl();
  if (overlayMode != OVERLAY_NONE) {  // main screen is redrawn when the view closes
    overlayDirty = true;
    return;
  }
  // Redraw everything that uses the colours (logos and weather icons are images).
  redrawMain(false);
}

void uiShowStationList(uint16_t count, uint16_t selected, UiStationName nameAt) {
  listCount = count;
  listSelected = count ? (uint16_t)(selected % count) : 0;
  listNameAt = nameAt;
  openOverlay(OVERLAY_STATIONS);
}

void uiShowVolume(uint8_t volume, uint8_t maxVolume, bool muted) {
  overlayVolume = volume > maxVolume ? maxVolume : volume;
  overlayVolumeMax = maxVolume;
  overlayMuted = muted;
  openOverlay(OVERLAY_VOLUME);
}

bool uiOverlayActive() { return overlayMode != OVERLAY_NONE; }

void uiShowSetup(const char *apSsid, const char *apPassword, const char *address, const char *note) {
  snprintf(setupSsid, sizeof(setupSsid), "%s", apSsid ? apSsid : "");
  snprintf(setupPassword, sizeof(setupPassword), "%s", apPassword ? apPassword : "");
  snprintf(setupAddress, sizeof(setupAddress), "%s", address ? address : "");
  snprintf(setupNote, sizeof(setupNote), "%s", note ? note : "");
  setupActive = true;
  if (overlayMode == OVERLAY_NONE || overlayMode == OVERLAY_SETUP) {
    overlayMode = OVERLAY_SETUP;
    overlayDirty = true;
  }  // an encoder screen open now returns to the setup screen when it closes
}

void uiHideSetup() {
  if (!setupActive) return;
  setupActive = false;
  if (overlayMode == OVERLAY_SETUP) {
    overlayMode = OVERLAY_NONE;
    redrawMain(true);
  }
}

const uint32_t *uiScreenRowVersions() { return shadowRowVersion; }

const uint16_t *uiScreenCopy(uint32_t &version) {
  version = shadowVersion;
  return shadow;
}

void uiLoop() {
  const uint32_t now = millis();
  if (overlayMode != OVERLAY_NONE) {
    if (overlayMode != OVERLAY_SETUP && now - overlayActivityAt >= OVERLAY_TIMEOUT_MS) {
      if (setupActive) {
        overlayMode = OVERLAY_SETUP;  // back to the setup screen, not to the main screen
        overlayDirty = false;
        renderOverlay();
      } else {
        overlayMode = OVERLAY_NONE;
        redrawMain(true);
      }
    } else if (overlayDirty) {
      overlayDirty = false;
      renderOverlay();
    }
  }
  tickLine(stationLine, now);
  tickLine(descriptionLine, now);
  tickLine(artistLine, now);
  tickLine(titleLine, now);
  tickVinyl(now);
}
