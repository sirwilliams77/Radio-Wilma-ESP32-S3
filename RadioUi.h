#pragma once
// Hi-Fi style screen for the ESP32-S3 internet radio (ST7796, 480x320).
// Every region is composed off-screen with antialiased Roboto text and sent to
// the display in a single transfer, and only when its content changes.
#include <stdint.h>
#include <time.h>

class Arduino_GFX;

bool uiBegin(Arduino_GFX *gfx);

// Top bar.
void uiSetWifi(bool connected, uint8_t signalBars);  // signalBars 0..4
void uiSetVolume(uint8_t volume, bool muted);
void uiSetTime(const struct tm *localTime);          // nullptr = waiting for NTP

// Station block. logo: RGB565 LOGO_W x LOGO_H bitmap or nullptr (placeholder); always redrawn.
void uiSetLogo(const uint16_t *logo);
void uiSetStationName(const char *name);
void uiSetDescription(const char *text);

// Track block.
void uiSetTrack(const char *artist, const char *title);
void uiSetPaused(bool paused);

// Bottom bar. source: first cell text (IP address of the radio),
// kbps 0 = unknown, genre nullptr/"" = unknown.
void uiSetBottomInfo(const char *source, uint8_t stationNumber, uint8_t stationCount,
                     uint16_t kbps, const char *genre, int16_t utcOffsetMinutes);

// Spectrum analyzer: 15 bands, values 0..255 (Audio evt_spectrum vec1/vec2). nullptr = silence.
void uiSetSpectrum(const uint8_t *bars, const uint8_t *peaks);

// Weather next to the clock. kind: UiWeatherKind, temperature in tenths of a degree C.
enum UiWeatherKind : uint8_t {
  UI_WX_CLEAR_DAY,
  UI_WX_CLEAR_NIGHT,
  UI_WX_PARTLY_DAY,
  UI_WX_PARTLY_NIGHT,
  UI_WX_CLOUDY,
  UI_WX_FOG,
  UI_WX_DRIZZLE,
  UI_WX_RAIN,
  UI_WX_SNOW,
  UI_WX_STORM,
};
void uiSetWeather(bool valid, uint8_t kind, int16_t temperatureTenths, const char *place);
// Own icon from the web panel: RGB565 78x60 kept by the caller, nullptr = built-in icon.
void uiSetWeatherIcon(uint8_t kind, const uint16_t *pixels);
const uint16_t *uiWeatherIcon(uint8_t kind);  // icon used now (own or built-in), 78x60 RGB565
bool uiWeatherIconIsOwn(uint8_t kind);

// Temporary screens opened by the encoders. Each call shows or refreshes the view and
// restarts its 2 s timer; afterwards uiLoop() returns to the main screen by itself.
// The main screen keeps receiving updates meanwhile and is redrawn completely.
typedef const char *(*UiStationName)(uint16_t index);  // station name, "" when out of range
void uiShowStationList(uint16_t count, uint16_t selected, UiStationName nameAt);
void uiShowVolume(uint8_t volume, uint8_t maxVolume, bool muted);
bool uiOverlayActive();

// Wi-Fi setup screen (setup access point active, no home network). Stays until uiHideSetup();
// the encoder screens may open over it and return to it. note: extra line, nullptr/"" = none.
void uiShowSetup(const char *apSsid, const char *apPassword, const char *address, const char *note);
void uiHideSetup();

// Adjustable colours (web panel: Ustawienia > Wygląd). Logos and weather icons are images.
enum UiColorSlot : uint8_t {
  UI_C_CLOCK, UI_C_COLON, UI_C_DATE, UI_C_DATE_ACCENT, UI_C_ACCENT, UI_C_VOLUME,
  UI_C_STATION, UI_C_DESC, UI_C_LABEL, UI_C_ARTIST, UI_C_TITLE, UI_C_TEMP, UI_C_CITY,
  UI_C_BOTTOM, UI_C_DIVIDER, UI_C_EQ_LOW, UI_C_EQ_MID, UI_C_EQ_HIGH, UI_C_FRAME,
  UI_C_COUNT
};
extern const uint32_t UI_DEFAULT_COLORS[UI_C_COUNT];  // 0xRRGGBB
void uiSetColors(const uint32_t *rgb);  // UI_C_COUNT entries (nullptr = defaults); redraws the screen

// Copy of the whole screen (480x320 RGB565, little-endian) for the web preview; nullptr when
// PSRAM is not available. version changes whenever anything on the screen changes.
const uint16_t *uiScreenCopy(uint32_t &version);
const uint32_t *uiScreenRowVersions();  // 320 entries: frame version of the last change of each row

// Call from loop(): scrolls lines that do not fit their field.
void uiLoop();
