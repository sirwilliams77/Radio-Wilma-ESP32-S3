#include <Arduino_GFX_Library.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <WiFi.h>
#include <time.h>
#include <string.h>
#include "Audio.h"
// Optional default Wi-Fi for the very first start (normally set in the web panel).
#if __has_include("secrets.h")
#include "secrets.h"
#endif
#ifndef WIFI_SSID
#define WIFI_SSID ""
#define WIFI_PASSWORD ""
#endif
#include "RadioApi.h"
#include "RadioUi.h"
#include "StationLogos.h"
#include "StationSearch.h"
#include "StationStore.h"
#include "WebPanel.h"
#include "Weather.h"

static const char *FIRMWARE_VERSION = "1.0.1";

// ESP32-S3 WROOM N16R8 + ST7796. Display wiring is unchanged from stage 2.
#define TFT_SCK 12
#define TFT_MOSI 11
#define TFT_MISO GFX_NOT_DEFINED
#define TFT_DC 9
#define TFT_CS 10
#define TFT_RST 14
#define BRIGHTNESS_PIN 21

// PCM5102A I2S pins provided for this project.
#define I2S_BCLK 17
#define I2S_LRC 15
#define I2S_DOUT 16

// Rotary encoders: S1, S2, KEY. Buttons are wired to GND (active low).
#define ENC_BTNL 5
#define ENC_BTNR 4
#define ENC_BTNB 6
#define ENC2_BTNL 18
#define ENC2_BTNR 7
#define ENC2_BTNB 3

// Keep the tested ST7796 + Arduino_GFX configuration unchanged.
Arduino_DataBus *displayBus = new Arduino_ESP32SPI(
    TFT_DC, TFT_CS, TFT_SCK, TFT_MOSI, TFT_MISO);
Arduino_GFX *display = new Arduino_ST7796(displayBus, TFT_RST, 1);
Audio audio;
Preferences preferences;

static const char *TIME_ZONE = "CET-1CEST,M3.5.0/2,M10.5.0/3";
static const char *NTP_SERVER_1 = "pool.ntp.org";
static const char *NTP_SERVER_2 = "time.google.com";

// Network: http://<HOSTNAME>.local, setup access point when no Wi-Fi is available.
// HOSTNAME: only a-z, 0-9 and "-" (no spaces, no Polish letters), e.g. "radio-salon".
// AP_SSID: any name up to 32 characters; AP_PASSWORD: 8-63 characters.
static const char *HOSTNAME = "radio";
static const char *AP_SSID = "Radio-Wilma";
static const char *AP_PASSWORD = "radio1234";
static const uint32_t AP_START_AFTER_MS = 45000;      // no Wi-Fi for this long -> setup AP
static const uint32_t AP_STOP_AFTER_CONNECT_MS = 60000;

// Used only when the station list on the flash does not exist yet (first start).
struct DefaultStation {
  const char *name;
  const char *url;
  const char *desc;
};
static const DefaultStation DEFAULT_STATIONS[] = {
    {"SomaFM Groove Salad", "https://somafm.com/groovesalad.pls", "Ambient i downtempo"},
    {"SomaFM Drone Zone", "https://somafm.com/dronezone.pls", "Muzyka ambientowa"}};

static const uint8_t VOLUME_STEPS = 21;
static const uint32_t WIFI_RETRY_INTERVAL_MS = 15000;
static const uint32_t STREAM_RETRY_INTERVAL_MS = 30000;
static const uint32_t RSSI_INTERVAL_MS = 2000;
static const uint32_t SETTINGS_SAVE_DELAY_MS = 5000;

// Stations and playback state
static std::vector<Station> stations;
static size_t stationIndex = 0;
static int pendingPlayIndex = -1;

// Station from the internet search (web panel): played without changing the favourites.
// It is not remembered; after a restart the last favourite station plays again.
static bool tempActive = false;
static Station tempStation;
static bool pendingTempPlay = false;

// Stream watchdog: readable messages instead of a silent "Łączenie...".
static const uint32_t STREAM_NO_AUDIO_MS = 15000;
static uint32_t streamConnectedAt = 0;
static bool streamAudioSeen = false;
static bool streamProblemShown = false;

static uint16_t *logoBuffer = nullptr;
static uint32_t logoRevision = 0;
static bool currentHasLogo = false;
static bool audioConfigured = false;
static bool stationPlaying = false;
static bool playbackPaused = false;
static bool muted = false;
static bool updating = false;
static uint8_t volumeLevel = 14;
static bool settingsDirty = false;
static uint32_t settingsChangedAt = 0;

// Network state
static String wifiSsid;
static String wifiPassword;
static String panelPassword;
static uint32_t lastWifiAttempt = 0;
static uint32_t lastStreamAttempt = 0;
static uint32_t lastRssiCheck = 0;
static uint32_t disconnectedSince = 0;
static uint32_t connectedAt = 0;
static bool ntpConfigured = false;
static bool lastWifiConnected = false;
static bool wifiStatusDrawn = false;
static bool apActive = false;
static bool mdnsStarted = false;
static uint8_t signalBars = 0;
static int lastShownMinute = -1;

// What is on the screen (also reported to the web panel)
static char shownStation[64] = "";
static char shownDescription[96] = "";
static char shownArtist[96] = "";
static char shownTitle[128] = "";

// Stream information shown in the bottom bar (updated from the audio callback).
static uint16_t streamKbps = 0;
static char streamGenre[48] = "";

// Metadata arrives from the audio library; it is copied here and drawn from loop().
static portMUX_TYPE metadataMux = portMUX_INITIALIZER_UNLOCKED;
static char pendingStationName[64] = "";
static char pendingDescription[96] = "";
static char pendingArtist[96] = "";
static char pendingTitle[128] = "";
static char pendingGenre[48] = "";
static uint32_t pendingBitrate = 0;
static volatile bool stationNamePending = false;
static volatile bool descriptionPending = false;
static volatile bool trackMetadataPending = false;
static volatile bool genrePending = false;
static volatile bool bitratePending = false;
static uint8_t pendingBars[15] = {0};
static uint8_t pendingPeaks[15] = {0};
static volatile bool spectrumPending = false;

struct EncoderState {
  uint8_t pinA;
  uint8_t pinB;
  uint8_t pinButton;  // KEY pin: configured, not used (the encoders only turn)
  uint8_t previousAB;
  int8_t quarterSteps;
};

static EncoderState volumeEncoder = {ENC_BTNL, ENC_BTNR, ENC_BTNB, 0, 0};     // encoder 1: volume only
static EncoderState stationEncoder = {ENC2_BTNL, ENC2_BTNR, ENC2_BTNB, 0, 0};  // encoder 2: station only
static const int8_t QUADRATURE_TABLE[16] = {
    0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0};

// ---------------------------------------------------------------------------
// Screen helpers
// ---------------------------------------------------------------------------
void showTrack(const char *artist, const char *title) {
  strlcpy(shownArtist, artist, sizeof(shownArtist));
  strlcpy(shownTitle, title, sizeof(shownTitle));
  uiSetTrack(artist, title);
}

void showStationName(const char *name) {
  strlcpy(shownStation, name, sizeof(shownStation));
  uiSetStationName(name);
}

void showDescription(const char *text) {
  strlcpy(shownDescription, text, sizeof(shownDescription));
  uiSetDescription(text);
}

int16_t utcOffsetMinutes() {
  const time_t now = time(nullptr);
  struct tm local, utc;
  localtime_r(&now, &local);
  gmtime_r(&now, &utc);
  int dayDiff = local.tm_yday - utc.tm_yday;
  if (dayDiff > 1) dayDiff = -1;   // year boundary
  if (dayDiff < -1) dayDiff = 1;
  return (int16_t)(dayDiff * 1440 + (local.tm_hour - utc.tm_hour) * 60 +
                   (local.tm_min - utc.tm_min));
}

// First bottom cell: the address of the panel for a while after connecting.
// First bottom cell: the current address of the radio (panel URL).
String sourceLabel() {
  if (WiFi.status() == WL_CONNECTED) return WiFi.localIP().toString();
  if (apActive) return WiFi.softAPIP().toString();
  return "Brak sieci";
}

void refreshBottomBar() {
  const uint8_t number = (stations.empty() || tempActive) ? 0 : (uint8_t)(stationIndex + 1);  // 0 = searched
  uiSetBottomInfo(sourceLabel().c_str(), number, (uint8_t)stations.size(), streamKbps,
                  streamGenre, utcOffsetMinutes());
}

uint8_t rssiToBars(int32_t rssi) {
  if (rssi >= -55) return 4;
  if (rssi >= -65) return 3;
  if (rssi >= -75) return 2;
  if (rssi >= -85) return 1;
  return 0;
}

void updateTopBar() {
  const bool connected = WiFi.status() == WL_CONNECTED;
  const uint32_t now = millis();
  if (now - lastRssiCheck >= RSSI_INTERVAL_MS || !connected) {
    lastRssiCheck = now;
    signalBars = connected ? rssiToBars(WiFi.RSSI()) : 0;
    refreshBottomBar();  // IP address may have changed
  }
  uiSetWifi(connected, signalBars);
  uiSetVolume(volumeLevel, muted);
}

void updateClockAndDate() {
  const time_t now = time(nullptr);
  if (now < 1700000000) {
    if (lastShownMinute != -2) {
      uiSetTime(nullptr);
      lastShownMinute = -2;
    }
    return;
  }
  struct tm localTime;
  if (localtime_r(&now, &localTime) == nullptr) return;
  const int minuteStamp = localTime.tm_yday * 1440 + localTime.tm_hour * 60 + localTime.tm_min;
  if (minuteStamp == lastShownMinute) return;
  lastShownMinute = minuteStamp;
  uiSetTime(&localTime);
  refreshBottomBar();  // UTC offset changes with summer/winter time
}

// Full-screen instructions while the setup access point is the only way to reach the radio.
void showSetupHint() {
  const String address = String("http://") + WiFi.softAPIP().toString();
  String note;
  if (!wifiSsid.isEmpty()) note = String("Radio nadal próbuje połączyć się z siecią \"") + wifiSsid + "\"";
  uiShowSetup(AP_SSID, AP_PASSWORD, address.c_str(), note.c_str());
}

// ---------------------------------------------------------------------------
// Settings (Preferences / NVS)
// ---------------------------------------------------------------------------
void loadSettings() {
  preferences.begin("radio", false);
  wifiSsid = preferences.getString("ssid", WIFI_SSID);
  wifiPassword = preferences.getString("pass", WIFI_PASSWORD);
  panelPassword = preferences.getString("panel", "");
  volumeLevel = preferences.getUChar("volume", volumeLevel);
  if (volumeLevel > VOLUME_STEPS) volumeLevel = VOLUME_STEPS;
}

void markSettingsDirty() {
  settingsDirty = true;
  settingsChangedAt = millis();
}

// ---------------------------------------------------------------------------
// Screen colours (web panel: Ustawienia > Wygląd)
// ---------------------------------------------------------------------------
static const char *const COLOR_KEYS[UI_C_COUNT] = {
    "clock", "colon", "date", "dateAccent", "accent", "volume", "station", "desc", "label",
    "artist", "title", "temp", "city", "bottom", "divider", "eqLow", "eqMid", "eqHigh", "frame"};
static uint32_t screenColors[UI_C_COUNT];
static bool colorsDirty = false;

void loadColors() {
  memcpy(screenColors, UI_DEFAULT_COLORS, sizeof(screenColors));
  // Older firmware saved fewer colours: keep them, new slots stay at their defaults.
  const size_t saved = preferences.getBytesLength("colors");
  if (saved > 0 && saved <= sizeof(screenColors) && saved % sizeof(uint32_t) == 0) {
    preferences.getBytes("colors", screenColors, saved);
  }
  uiSetColors(screenColors);
}

static String hexColor(uint32_t c) {
  char text[8];
  snprintf(text, sizeof(text), "#%06lx", (unsigned long)(c & 0xFFFFFF));
  return String(text);
}

String radioColorsJson() {
  String json = "{\"colors\":{";
  for (uint8_t i = 0; i < UI_C_COUNT; ++i) {
    if (i) json += ',';
    json += "\"" + String(COLOR_KEYS[i]) + "\":\"" + hexColor(screenColors[i]) + "\"";
  }
  json += "},\"defaults\":{";
  for (uint8_t i = 0; i < UI_C_COUNT; ++i) {
    if (i) json += ',';
    json += "\"" + String(COLOR_KEYS[i]) + "\":\"" + hexColor(UI_DEFAULT_COLORS[i]) + "\"";
  }
  json += "}}";
  return json;
}

// value(key) returns "#rrggbb" or "" when the key is not part of the request.
bool radioSetColors(const std::function<String(const char *)> &value, bool reset) {
  bool changed = false;
  for (uint8_t i = 0; i < UI_C_COUNT; ++i) {
    uint32_t c = UI_DEFAULT_COLORS[i];
    if (!reset) {
      const String v = value(COLOR_KEYS[i]);
      if (v.length() != 7 || v[0] != '#') continue;
      char *end = nullptr;
      c = strtoul(v.c_str() + 1, &end, 16);
      if (end == nullptr || *end != '\0') continue;
    }
    if (screenColors[i] != c) {
      screenColors[i] = c;
      changed = true;
    }
  }
  if (changed) {
    uiSetColors(screenColors);
    colorsDirty = true;
    markSettingsDirty();
  }
  return changed;
}

void saveSettingsIfNeeded() {
  if (!settingsDirty || millis() - settingsChangedAt < SETTINGS_SAVE_DELAY_MS) return;
  settingsDirty = false;
  if (colorsDirty) {
    colorsDirty = false;
    preferences.putBytes("colors", screenColors, sizeof(screenColors));
  }
  preferences.putUChar("volume", volumeLevel);
  if (!stations.empty()) preferences.putUInt("station", stations[stationIndex].id);
}

// ---------------------------------------------------------------------------
// Stations
// ---------------------------------------------------------------------------
void loadStations() {
  storeBegin();
  if (storeLoad(stations)) {
    Serial.printf("Stations loaded from flash: %u\n", (unsigned)stations.size());
  } else {
    stations.clear();
    uint32_t id = 1;
    for (const DefaultStation &d : DEFAULT_STATIONS) {
      stations.push_back({id++, d.name, d.url, d.desc});
    }
    if (storeReady()) {
      storeSave(stations);
      logoSave(stations[0].id, (const uint8_t *)LOGO_TEST, LOGO_BYTES);  // test graphic
      Serial.println("Default station list written to flash");
    } else {
      Serial.println("FAT partition not available - using built-in stations");
    }
  }
  const uint32_t savedId = preferences.getUInt("station", 0);
  stationIndex = 0;
  for (size_t i = 0; i < stations.size(); ++i) {
    if (stations[i].id == savedId) stationIndex = i;
  }
}

bool haveCurrentStation() { return tempActive || !stations.empty(); }
const Station &currentStation() { return tempActive ? tempStation : stations[stationIndex]; }

void showLogoForCurrentStation() {
  currentHasLogo = !tempActive && !stations.empty() && logoLoad(stations[stationIndex].id, logoBuffer);
  uiSetLogo(currentHasLogo ? logoBuffer : nullptr);
  ++logoRevision;
}

void showSelectedStation() {
  portENTER_CRITICAL(&metadataMux);
  pendingStationName[0] = '\0';
  pendingDescription[0] = '\0';
  pendingArtist[0] = '\0';
  pendingTitle[0] = '\0';
  pendingGenre[0] = '\0';
  stationNamePending = false;
  descriptionPending = false;
  trackMetadataPending = false;
  genrePending = false;
  bitratePending = false;
  portEXIT_CRITICAL(&metadataMux);
  streamKbps = 0;
  streamGenre[0] = '\0';
  showLogoForCurrentStation();
  if (!haveCurrentStation()) {
    showStationName("Brak stacji");
    showDescription("Dodaj stacje w panelu WWW");
    const String address = String("http://") + HOSTNAME + ".local";
    showTrack("Lista stacji jest pusta", address.c_str());
  } else {
    const Station &station = currentStation();
    showStationName(station.name.c_str());
    showDescription(station.desc.c_str());
    showTrack("Łączenie...", "Oczekiwanie na utwór");
  }
  refreshBottomBar();
}

void connectSelectedStation() {
  if (!audioConfigured || updating || WiFi.status() != WL_CONNECTED) return;
  audio.stopSong();
  stationPlaying = false;
  playbackPaused = false;
  uiSetPaused(false);
  uiSetSpectrum(nullptr, nullptr);
  showSelectedStation();
  if (!haveCurrentStation()) return;
  const Station &station = currentStation();
  Serial.printf("Connecting to %s station: %s\n", tempActive ? "searched" : "favourite", station.name.c_str());
  lastStreamAttempt = millis();
  streamAudioSeen = false;
  streamProblemShown = false;
  stationPlaying = audio.connecttohost(station.url.c_str());
  streamConnectedAt = millis();
  if (!stationPlaying) {
    showTrack("Strumień niedostępny", "Ponowna próba za 30 s");
    streamProblemShown = true;
  }
}

// Called from loop(): explains a stream that gives no sound or stops.
void checkStreamHealth() {
  if (!audioConfigured || updating || playbackPaused || !stationPlaying || streamProblemShown) return;
  if (WiFi.status() != WL_CONNECTED) return;
  const uint32_t now = millis();
  if (!streamAudioSeen && now - streamConnectedAt >= STREAM_NO_AUDIO_MS) {
    showTrack("Brak dźwięku ze strumienia", "Strumień niedostępny lub format nieobsługiwany");
    streamProblemShown = true;
  } else if (streamAudioSeen && !audio.isRunning()) {
    showTrack("Połączenie ze strumieniem przerwane", "Ponowna próba za chwilę");
    streamProblemShown = true;
  }
}

void selectStationIndex(size_t index) {
  if (stations.empty()) return;
  tempActive = false;  // back to the favourites
  stationIndex = index % stations.size();
  markSettingsDirty();
  if (WiFi.status() == WL_CONNECTED) connectSelectedStation();
  else showSelectedStation();
}

void selectStation(int8_t direction) {
  if (direction == 0 || stations.empty()) return;
  const size_t count = stations.size();
  selectStationIndex((stationIndex + count + (direction > 0 ? 1 : count - 1)) % count);
}

// ---------------------------------------------------------------------------
// Metadata (UTF-8 is kept: the fonts include Polish and Western European letters)
// ---------------------------------------------------------------------------
void copyUtf8(char *destination, size_t capacity, const char *source) {
  if (capacity == 0) return;
  size_t out = 0;
  while (*source) {
    const uint8_t c = (uint8_t)*source;
    size_t length = 1;
    if (c >= 0xF0) length = 4;
    else if (c >= 0xE0) length = 3;
    else if (c >= 0xC0) length = 2;
    else if (c >= 0x80) length = 0;  // stray continuation byte or Latin-1
    bool valid = length > 0;
    for (size_t i = 1; valid && i < length; ++i) {
      valid = ((uint8_t)source[i] & 0xC0) == 0x80;
    }
    if (!valid) {
      if (out + 2 > capacity) break;
      destination[out++] = '?';
      ++source;
      continue;
    }
    if (c < 0x20) {  // control characters
      ++source;
      continue;
    }
    if (out + length + 1 > capacity) break;  // never cut a character in half
    memcpy(destination + out, source, length);
    out += length;
    source += length;
  }
  destination[out] = '\0';
}

// Some stations send escaped text: JSON (\u0027, \") or HTML (&amp;, &#39;).
// Decodes it in place into UTF-8 (the result is never longer than the input).
static size_t putUtf8(char *out, uint32_t cp) {
  if (cp < 0x80) { out[0] = (char)cp; return 1; }
  if (cp < 0x800) { out[0] = (char)(0xC0 | (cp >> 6)); out[1] = (char)(0x80 | (cp & 0x3F)); return 2; }
  if (cp < 0x10000) {
    out[0] = (char)(0xE0 | (cp >> 12)); out[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
    out[2] = (char)(0x80 | (cp & 0x3F)); return 3;
  }
  out[0] = (char)(0xF0 | (cp >> 18)); out[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
  out[2] = (char)(0x80 | ((cp >> 6) & 0x3F)); out[3] = (char)(0x80 | (cp & 0x3F)); return 4;
}

static bool hex4(const char *p, uint32_t &v) {
  v = 0;
  for (int i = 0; i < 4; ++i) {
    const char c = p[i];
    v <<= 4;
    if (c >= '0' && c <= '9') v |= c - '0';
    else if (c >= 'a' && c <= 'f') v |= c - 'a' + 10;
    else if (c >= 'A' && c <= 'F') v |= c - 'A' + 10;
    else return false;
  }
  return true;
}

void decodeEscapes(char *text) {
  static const struct { const char *name; char ch; } ENTITIES[] = {
      {"&amp;", '&'}, {"&lt;", '<'}, {"&gt;", '>'}, {"&quot;", '"'}, {"&apos;", '\''}, {"&nbsp;", ' '}};
  const char *in = text;
  char *out = text;
  while (*in) {
    uint32_t cp = 0;
    if (in[0] == '\\' && in[1] == 'u' && hex4(in + 2, cp)) {
      in += 6;
      uint32_t low = 0;
      if (cp >= 0xD800 && cp <= 0xDBFF && in[0] == '\\' && in[1] == 'u' && hex4(in + 2, low) &&
          low >= 0xDC00 && low <= 0xDFFF) {
        cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
        in += 6;
      }
      if (cp >= 0x20) out += putUtf8(out, cp);
      continue;
    }
    if (in[0] == '\\' && (in[1] == '"' || in[1] == '\\' || in[1] == '/' || in[1] == '\'')) {
      *out++ = in[1];
      in += 2;
      continue;
    }
    if (in[0] == '&' && in[1] == '#') {
      const bool hex = in[2] == 'x' || in[2] == 'X';
      char *end = nullptr;
      const unsigned long v = strtoul(in + (hex ? 3 : 2), &end, hex ? 16 : 10);
      if (end && *end == ';' && end > in + (hex ? 3 : 2) && v >= 0x20 && v <= 0x10FFFF) {
        out += putUtf8(out, (uint32_t)v);
        in = end + 1;
        continue;
      }
    }
    if (in[0] == '&') {
      bool matched = false;
      for (const auto &e : ENTITIES) {
        const size_t n = strlen(e.name);
        if (strncmp(in, e.name, n) == 0) {
          *out++ = e.ch;
          in += n;
          matched = true;
          break;
        }
      }
      if (matched) continue;
    }
    *out++ = *in++;
  }
  *out = '\0';
}

// Removes "Station name: " (or "Station name - ") from the start of the artist.
void stripStationPrefix(char *artist, const char *stationName) {
  const size_t n = strlen(stationName);
  if (n == 0 || strncasecmp(artist, stationName, n) != 0) return;
  const char *rest = artist + n;
  while (*rest == ' ') ++rest;
  if (*rest != ':' && *rest != '-' && *rest != '|') return;
  ++rest;
  while (*rest == ' ') ++rest;
  if (*rest == '\0') return;  // nothing left - keep the original text
  memmove(artist, rest, strlen(rest) + 1);
}

void trimSpaces(char *text) {
  size_t length = strlen(text);
  while (length > 0 && text[length - 1] == ' ') text[--length] = '\0';
  size_t start = 0;
  while (text[start] == ' ') ++start;
  if (start) memmove(text, text + start, length - start + 1);
}

void audioInfoCallback(Audio::msg_t message) {
  if (message.msg == nullptr) return;
  switch (message.e) {
    case Audio::evt_name:
      portENTER_CRITICAL(&metadataMux);
      copyUtf8(pendingStationName, sizeof(pendingStationName), message.msg);
      decodeEscapes(pendingStationName);
      stationNamePending = true;
      portEXIT_CRITICAL(&metadataMux);
      break;
    case Audio::evt_icydescription:
      portENTER_CRITICAL(&metadataMux);
      copyUtf8(pendingDescription, sizeof(pendingDescription), message.msg);
      decodeEscapes(pendingDescription);
      descriptionPending = true;
      portEXIT_CRITICAL(&metadataMux);
      break;
    case Audio::evt_genre:
      portENTER_CRITICAL(&metadataMux);
      copyUtf8(pendingGenre, sizeof(pendingGenre), message.msg);
      genrePending = true;
      portEXIT_CRITICAL(&metadataMux);
      break;
    case Audio::evt_bitrate:
      portENTER_CRITICAL(&metadataMux);
      pendingBitrate = (uint32_t)message.arg1;  // bits per second
      bitratePending = true;
      portEXIT_CRITICAL(&metadataMux);
      break;
    case Audio::evt_spectrum:
      if (message.vec1.size() >= 15 && message.vec2.size() >= 15) {
        portENTER_CRITICAL(&metadataMux);
        for (uint8_t i = 0; i < 15; ++i) {
          pendingBars[i] = (uint8_t)message.vec1[i];
          pendingPeaks[i] = (uint8_t)message.vec2[i];
        }
        spectrumPending = true;
        portEXIT_CRITICAL(&metadataMux);
      }
      break;
    case Audio::evt_streamtitle: {
      char normalized[224];
      copyUtf8(normalized, sizeof(normalized), message.msg);
      decodeEscapes(normalized);
      char artist[96] = "";
      char title[128] = "";
      char *separator = strstr(normalized, " - ");
      if (separator != nullptr) {
        *separator = '\0';
        copyUtf8(artist, sizeof(artist), normalized);
        copyUtf8(title, sizeof(title), separator + 3);
      } else {
        copyUtf8(title, sizeof(title), normalized);
      }
      trimSpaces(artist);
      trimSpaces(title);
      portENTER_CRITICAL(&metadataMux);
      strlcpy(pendingArtist, artist, sizeof(pendingArtist));
      strlcpy(pendingTitle, title, sizeof(pendingTitle));
      trackMetadataPending = true;
      portEXIT_CRITICAL(&metadataMux);
      break;
    }
    default:
      break;
  }
}

void applyPendingMetadata() {
  char station[64] = "";
  char description[96] = "";
  char artist[96] = "";
  char title[128] = "";
  char genre[48] = "";
  uint32_t bitrate = 0;
  uint8_t bars[15], peaks[15];
  bool haveSpectrum = false;
  bool haveStation = false, haveDescription = false, haveTrack = false;
  bool haveGenre = false, haveBitrate = false;

  portENTER_CRITICAL(&metadataMux);
  if (stationNamePending) {
    strlcpy(station, pendingStationName, sizeof(station));
    stationNamePending = false;
    haveStation = true;
  }
  if (descriptionPending) {
    strlcpy(description, pendingDescription, sizeof(description));
    descriptionPending = false;
    haveDescription = true;
  }
  if (trackMetadataPending) {
    strlcpy(artist, pendingArtist, sizeof(artist));
    strlcpy(title, pendingTitle, sizeof(title));
    trackMetadataPending = false;
    haveTrack = true;
  }
  if (genrePending) {
    strlcpy(genre, pendingGenre, sizeof(genre));
    genrePending = false;
    haveGenre = true;
  }
  if (bitratePending) {
    bitrate = pendingBitrate;
    bitratePending = false;
    haveBitrate = true;
  }
  if (spectrumPending) {
    memcpy(bars, pendingBars, sizeof(bars));
    memcpy(peaks, pendingPeaks, sizeof(peaks));
    spectrumPending = false;
    haveSpectrum = true;
  }
  portEXIT_CRITICAL(&metadataMux);
  if (updating) return;

  if (haveSpectrum && stationPlaying && !playbackPaused) uiSetSpectrum(bars, peaks);
  if (haveSpectrum && !streamAudioSeen) {
    for (uint8_t b = 0; b < 15; ++b) streamAudioSeen = streamAudioSeen || bars[b] > 0;
  }
  if (haveBitrate && bitrate > 0) streamAudioSeen = true;

  if (haveStation && station[0] != '\0') showStationName(station);
  if (haveDescription) {
    trimSpaces(description);
    // Some streams send a placeholder description; keep the configured one then.
    if (description[0] != '\0' && strcmp(description, "Unspecified description") != 0) {
      showDescription(description);
    }
  }
  if (haveTrack) {
    stripStationPrefix(artist, shownStation);
    if (haveCurrentStation()) stripStationPrefix(artist, currentStation().name.c_str());
    if (artist[0] == '\0') strlcpy(artist, "Nieznany wykonawca", sizeof(artist));
    if (title[0] == '\0') strlcpy(title, "Brak informacji o utworze", sizeof(title));
    showTrack(artist, title);
  }
  if (haveGenre || haveBitrate) {
    if (haveGenre) {
      trimSpaces(genre);
      strlcpy(streamGenre, genre, sizeof(streamGenre));
    }
    if (haveBitrate && bitrate > 0) {
      // Bitrate arrives in b/s (some streams report kb/s directly).
      streamKbps = (uint16_t)(bitrate >= 1000 ? (bitrate + 500) / 1000 : bitrate);
    }
    refreshBottomBar();
  }
}

// ---------------------------------------------------------------------------
// Wi-Fi, setup access point, mDNS, web panel
// ---------------------------------------------------------------------------
void startAccessPoint() {
  if (apActive) return;
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  apActive = true;
  webCaptivePortal(true);
  Serial.printf("Setup access point %s, panel at http://%s\n", AP_SSID,
                WiFi.softAPIP().toString().c_str());
  if (WiFi.status() != WL_CONNECTED) showSetupHint();
  refreshBottomBar();
}

void stopAccessPoint() {
  if (!apActive) return;
  webCaptivePortal(false);
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  apActive = false;
  uiHideSetup();
  Serial.println("Setup access point stopped");
}

void beginStation() {
  if (wifiSsid.isEmpty()) return;
  Serial.printf("Connecting to Wi-Fi %s...\n", wifiSsid.c_str());
  WiFi.begin(wifiSsid.c_str(), wifiPassword.c_str());
  lastWifiAttempt = millis();
}

void updateWifi() {
  const uint32_t now = millis();
  const bool connected = WiFi.status() == WL_CONNECTED;

  if (connected) {
    disconnectedSince = 0;
    if (apActive && now - connectedAt >= AP_STOP_AFTER_CONNECT_MS) stopAccessPoint();
  } else {
    if (disconnectedSince == 0) disconnectedSince = now;
    if (!apActive && (wifiSsid.isEmpty() || now - disconnectedSince >= AP_START_AFTER_MS)) {
      startAccessPoint();
    }
    // While someone is connected to the setup network, do not scan for the home
    // network: that would switch channels and drop the phone from the access point.
    const bool apInUse = apActive && WiFi.softAPgetStationNum() > 0;
    const uint32_t retry = apActive ? 2 * WIFI_RETRY_INTERVAL_MS : WIFI_RETRY_INTERVAL_MS;
    if (!apInUse && (lastWifiAttempt == 0 || now - lastWifiAttempt >= retry)) beginStation();
  }

  if (connected && !ntpConfigured) {
    configTzTime(TIME_ZONE, NTP_SERVER_1, NTP_SERVER_2);
    ntpConfigured = true;
    Serial.println("NTP synchronization requested");
  }
  if (!wifiStatusDrawn || connected != lastWifiConnected) {
    lastWifiConnected = connected;
    wifiStatusDrawn = true;
    if (connected) {
      connectedAt = now;
      uiHideSetup();  // the access point stays a while longer, the home network works
      Serial.printf("Wi-Fi connected, panel: http://%s.local or http://%s\n", HOSTNAME,
                    WiFi.localIP().toString().c_str());
      if (!mdnsStarted && MDNS.begin(HOSTNAME)) {
        MDNS.addService("http", "tcp", 80);
        mdnsStarted = true;
      }
    } else {
      Serial.println("Wi-Fi disconnected");
      if (apActive) showSetupHint();
    }
    if (!connected && audioConfigured) {
      audio.stopSong();
      stationPlaying = false;
      uiSetSpectrum(nullptr, nullptr);
      if (haveCurrentStation()) showTrack("Brak połączenia z siecią Wi-Fi", "Odtwarzanie wznowi się samo");
    }
    if (connected && audioConfigured) connectSelectedStation();
    refreshBottomBar();
  }
  if (connected && audioConfigured && !updating && !playbackPaused && haveCurrentStation() &&
      (!stationPlaying || !audio.isRunning()) &&
      now - lastStreamAttempt >= STREAM_RETRY_INTERVAL_MS) {
    connectSelectedStation();
  }
}

// ---------------------------------------------------------------------------
// RadioApi (used by the web panel)
// ---------------------------------------------------------------------------
String radioStatusJson() {
  const bool connected = WiFi.status() == WL_CONNECTED;
  const bool haveStation = !stations.empty();
  String json = "{";
  json += "\"index\":" + String(haveStation ? (int)stationIndex : -1);
  json += ",\"count\":" + String(stations.size());
  json += ",\"id\":" + String(haveStation && !tempActive ? stations[stationIndex].id : 0);
  json += ",\"temp\":" + String(tempActive ? "true" : "false");
  json += ",\"logo\":" + String(currentHasLogo ? "true" : "false");
  json += ",\"logoRev\":" + String(logoRevision);
  json += ",\"station\":\"" + jsonEscape(shownStation) + "\"";
  json += ",\"desc\":\"" + jsonEscape(shownDescription) + "\"";
  json += ",\"artist\":\"" + jsonEscape(shownArtist) + "\"";
  json += ",\"title\":\"" + jsonEscape(shownTitle) + "\"";
  json += ",\"kbps\":" + String(streamKbps);
  json += ",\"genre\":\"" + jsonEscape(streamGenre) + "\"";
  json += ",\"paused\":" + String(playbackPaused ? "true" : "false");
  json += ",\"muted\":" + String(muted ? "true" : "false");
  json += ",\"volume\":" + String(volumeLevel);
  json += ",\"maxVolume\":" + String(VOLUME_STEPS);
  json += ",\"weather\":" + weatherStatusJson();
  json += ",\"version\":\"" + String(FIRMWARE_VERSION) + "\"";
  json += ",\"wifi\":{\"connected\":" + String(connected ? "true" : "false");
  json += ",\"ip\":\"" + (connected ? WiFi.localIP().toString() : WiFi.softAPIP().toString()) + "\"}";
  json += "}";
  return json;
}

void radioPlayIndex(int index) { pendingPlayIndex = index; }

void radioStep(int direction) {
  if (stations.empty()) return;
  const size_t count = stations.size();
  pendingPlayIndex = (int)((stationIndex + count + (direction > 0 ? 1 : count - 1)) % count);
}

void radioTogglePause() {
  if (stations.empty()) return;
  playbackPaused = !playbackPaused;
  audio.pauseResume();
  uiSetPaused(playbackPaused);
  if (playbackPaused) uiSetSpectrum(nullptr, nullptr);
}

void radioSetVolume(int volume) {
  if (volume < 0) volume = 0;
  if (volume > VOLUME_STEPS) volume = VOLUME_STEPS;
  volumeLevel = (uint8_t)volume;
  audio.setVolume(volumeLevel);
  if (muted) {
    muted = false;
    audio.setMute(false);
  }
  markSettingsDirty();
}

void radioToggleMute() {
  muted = !muted;
  audio.setMute(muted);
}

const std::vector<Station> &radioStations() { return stations; }

bool radioReplaceStations(const std::vector<Station> &newStations, String &error) {
  const uint32_t currentId = stations.empty() ? 0 : stations[stationIndex].id;
  const String oldName = stations.empty() ? String() : stations[stationIndex].name;
  const String oldDesc = stations.empty() ? String() : stations[stationIndex].desc;
  if (!storeSave(newStations)) {
    error = "Nie można zapisać listy w pamięci flash: " + storeLastError();
    return false;
  }
  stations = newStations;
  logoCleanup(stations);

  // Keep playing the same station if it is still on the list.
  for (size_t i = 0; i < stations.size(); ++i) {
    if (stations[i].id == currentId) {
      stationIndex = i;
      // Show edited name/description at once (stream metadata may replace them later).
      if (stations[i].name != oldName) showStationName(stations[i].name.c_str());
      if (stations[i].desc != oldDesc) showDescription(stations[i].desc.c_str());
      refreshBottomBar();
      markSettingsDirty();
      return true;
    }
  }
  stationIndex = 0;
  markSettingsDirty();
  if (WiFi.status() == WL_CONNECTED && !stations.empty()) {
    pendingPlayIndex = 0;
  } else {
    audio.stopSong();
    stationPlaying = false;
    showSelectedStation();
  }
  return true;
}

// Own weather icons from the web panel (PSRAM, allocated only for replaced icons).
static uint16_t *weatherIcons[WEATHER_ICON_KINDS] = {nullptr};

void radioWeatherIconChanged(uint8_t kind) {
  if (kind >= WEATHER_ICON_KINDS) return;
  if (weatherIcons[kind] == nullptr) {
    weatherIcons[kind] = (uint16_t *)ps_malloc(WEATHER_ICON_BYTES);
    if (weatherIcons[kind] == nullptr) weatherIcons[kind] = (uint16_t *)malloc(WEATHER_ICON_BYTES);
  }
  uiSetWeatherIcon(kind, nullptr);  // built-in icon while the file is read
  if (weatherIcons[kind] != nullptr && weatherIconLoad(kind, weatherIcons[kind])) {
    uiSetWeatherIcon(kind, weatherIcons[kind]);
  } else if (weatherIcons[kind] != nullptr) {
    free(weatherIcons[kind]);
    weatherIcons[kind] = nullptr;
  }
}

// Internet search (web panel) --------------------------------------------------
// Description shown under the station name: country and the first genres.
static String searchDescription(const SearchResult &r) {
  String text = r.country;
  if (r.tags[0]) {
    if (!text.isEmpty()) text += " \xC2\xB7 ";  // middle dot
    text += r.tags;
  }
  char fitted[STATION_DESC_MAX + 1];
  copyUtf8(fitted, sizeof(fitted), text.c_str());
  return String(fitted);
}

bool radioPlaySearchResult(uint8_t index, String &error) {
  SearchResult r;
  if (!searchGetResult(index, r)) {
    error = "Wyniki są nieaktualne, wyszukaj ponownie";
    return false;
  }
  if (!r.playable) {
    error = r.reason[0] ? String(r.reason) : String("Nieobsługiwany strumień");
    return false;
  }
  if (WiFi.status() != WL_CONNECTED) {
    error = "Brak połączenia z internetem";
    return false;
  }
  char name[STATION_NAME_MAX + 1];
  copyUtf8(name, sizeof(name), r.name);
  tempStation.id = 0;
  tempStation.name = name;
  tempStation.url = r.url;
  tempStation.desc = searchDescription(r);
  pendingTempPlay = true;  // connects in loop(), not inside the web request
  return true;
}

bool radioAddSearchResult(uint8_t index, String &error) {
  SearchResult r;
  if (!searchGetResult(index, r)) {
    error = "Wyniki są nieaktualne, wyszukaj ponownie";
    return false;
  }
  if (!r.playable) {
    error = r.reason[0] ? String(r.reason) : String("Nieobsługiwany strumień");
    return false;
  }
  if (stations.size() >= MAX_STATIONS) {
    error = "Lista ulubionych jest pełna (" + String(MAX_STATIONS) + " stacji)";
    return false;
  }
  uint32_t maxId = 0;
  for (const Station &s : stations) {
    if (s.url == r.url) {
      error = "Ta stacja jest już na liście ulubionych";
      return false;
    }
    if (s.id > maxId) maxId = s.id;
  }
  char name[STATION_NAME_MAX + 1];
  copyUtf8(name, sizeof(name), r.name);
  Station added;
  added.id = maxId + 1;
  added.name = name;
  added.url = r.url;
  added.desc = searchDescription(r);
  std::vector<Station> list = stations;
  list.push_back(added);
  if (!radioReplaceStations(list, error)) return false;
  // Adding the station that plays now makes it a normal favourite (encoder, restart).
  if (tempActive && tempStation.url == added.url) {
    tempActive = false;
    stationIndex = stations.size() - 1;
    markSettingsDirty();
    refreshBottomBar();
  }
  return true;
}

void radioLogoChanged(uint32_t stationId) {
  if (!stations.empty() && stations[stationIndex].id == stationId) showLogoForCurrentStation();
}

String radioSettingsJson() {
  const bool connected = WiFi.status() == WL_CONNECTED;
  String json = "{";
  json += "\"connected\":" + String(connected ? "true" : "false");
  json += ",\"ssid\":\"" + jsonEscape(wifiSsid) + "\"";
  json += ",\"ip\":\"" + (connected ? WiFi.localIP().toString() : String("")) + "\"";
  json += ",\"rssi\":" + String(connected ? WiFi.RSSI() : 0);
  json += ",\"apActive\":" + String(apActive ? "true" : "false");
  json += ",\"apSsid\":\"" + String(AP_SSID) + "\"";
  json += ",\"apIp\":\"" + WiFi.softAPIP().toString() + "\"";
  json += ",\"panelPassword\":" + String(panelPassword.isEmpty() ? "false" : "true");
  json += ",\"hostname\":\"" + String(HOSTNAME) + "\"";
  json += ",\"version\":\"" + jsonEscape(FIRMWARE_VERSION) + "\"";
  json += ",\"heap\":" + String(ESP.getFreeHeap());
  json += ",\"psram\":" + String(ESP.getFreePsram());
  json += ",\"fsUsed\":" + String(storeUsedBytes());
  json += ",\"fsTotal\":" + String(storeTotalBytes());
  json += ",\"weather\":" + weatherStatusJson();
  json += ",\"fsInfo\":\"" + jsonEscape(storeReady() ? storeDescription() : storeLastError()) + "\"";
  json += "}";
  return json;
}

void radioSetWifi(const String &ssid, const String &password) {
  wifiSsid = ssid;
  wifiPassword = password;
  preferences.putString("ssid", wifiSsid);
  preferences.putString("pass", wifiPassword);
  WiFi.disconnect(false);
  beginStation();
}

void radioSetPanelPassword(const String &password) {
  panelPassword = password;
  preferences.putString("panel", panelPassword);
}

const String &radioPanelPassword() { return panelPassword; }

void radioUpdateStarted() {
  updating = true;
  audio.stopSong();
  stationPlaying = false;
  uiSetSpectrum(nullptr, nullptr);
  showTrack("Aktualizacja oprogramowania", "Nie wyłączaj radia...");
}

void radioUpdateProgress(size_t written, size_t total) {
  static uint8_t lastPercent = 255;
  if (total == 0) return;
  const uint8_t percent = (uint8_t)min((size_t)100, written * 100 / total);
  if (percent / 5 == lastPercent / 5) return;
  lastPercent = percent;
  char text[48];
  snprintf(text, sizeof(text), "Postęp: %u%%", percent);
  showTrack("Aktualizacja oprogramowania", text);
}

void radioUpdateFailed(const String &reason) {
  updating = false;
  showTrack("Aktualizacja nieudana", reason.c_str());
  lastStreamAttempt = millis();  // resume playback after the retry interval
}

// ---------------------------------------------------------------------------
// Encoders
// ---------------------------------------------------------------------------
int8_t readEncoderStep(EncoderState &encoder) {
  const uint8_t currentAB = (digitalRead(encoder.pinA) << 1) | digitalRead(encoder.pinB);
  const uint8_t tableIndex = (encoder.previousAB << 2) | currentAB;
  encoder.previousAB = currentAB;
  const int8_t transition = QUADRATURE_TABLE[tableIndex];
  encoder.quarterSteps += transition;
  if (encoder.quarterSteps >= 4) {
    encoder.quarterSteps = 0;
    return 1;
  }
  if (encoder.quarterSteps <= -4) {
    encoder.quarterSteps = 0;
    return -1;
  }
  return 0;
}

void changeVolume(int8_t direction) {
  int16_t nextVolume = (int16_t)volumeLevel + direction;
  if (nextVolume < 0) nextVolume = 0;
  if (nextVolume > VOLUME_STEPS) nextVolume = VOLUME_STEPS;
  if (nextVolume == volumeLevel) return;
  volumeLevel = (uint8_t)nextVolume;
  audio.setVolume(volumeLevel);
  markSettingsDirty();
}

// Station list on the screen (encoders). The highlight moves at once; the connection starts
// when the knob rests for a moment, so fast turning is not slowed down by connecting to
// every station on the way (connecting blocks the loop and the encoders are polled).
static const uint32_t ENCODER_CONNECT_DELAY_MS = 250;
static bool encoderConnectPending = false;
static uint32_t encoderConnectAt = 0;

const char *stationNameAt(uint16_t index) {
  return index < stations.size() ? stations[index].name.c_str() : "";
}

void encoderSelectStation(int8_t direction) {
  if (direction == 0 || stations.empty()) return;
  const size_t count = stations.size();
  stationIndex = (stationIndex + count + (direction > 0 ? 1 : count - 1)) % count;
  uiShowStationList((uint16_t)count, (uint16_t)stationIndex, stationNameAt);
  encoderConnectPending = true;
  encoderConnectAt = millis() + ENCODER_CONNECT_DELAY_MS;
}

void startPendingEncoderStation() {
  if (!encoderConnectPending || (int32_t)(millis() - encoderConnectAt) < 0) return;
  encoderConnectPending = false;
  selectStationIndex(stationIndex);  // saves the choice and starts playback
}

// Encoder 1 changes only the volume, encoder 2 only the station. The buttons have no function.
void handleEncoders() {
  const int8_t volumeTurn = readEncoderStep(volumeEncoder);
  const int8_t stationTurn = readEncoderStep(stationEncoder);
  if (updating) return;

  if (volumeTurn != 0) {
    changeVolume(volumeTurn);
    uiShowVolume(volumeLevel, VOLUME_STEPS, muted);
  }
  if (stationTurn != 0) encoderSelectStation(stationTurn);
  startPendingEncoderStation();
}

void setupEncoders() {
  pinMode(ENC_BTNL, INPUT_PULLUP);
  pinMode(ENC_BTNR, INPUT_PULLUP);
  pinMode(ENC_BTNB, INPUT_PULLUP);
  pinMode(ENC2_BTNL, INPUT_PULLUP);
  pinMode(ENC2_BTNR, INPUT_PULLUP);
  pinMode(ENC2_BTNB, INPUT_PULLUP);
  volumeEncoder.previousAB = (digitalRead(ENC_BTNL) << 1) | digitalRead(ENC_BTNR);
  stationEncoder.previousAB = (digitalRead(ENC2_BTNL) << 1) | digitalRead(ENC2_BTNR);
}

// ---------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.printf("Radio Wilma %s\n", FIRMWARE_VERSION);
  pinMode(BRIGHTNESS_PIN, OUTPUT);
  digitalWrite(BRIGHTNESS_PIN, HIGH);

  if (!display->begin()) {
    Serial.println("Display initialization failed");
    while (true) delay(1000);
  }
  // Local time rules are known before NTP, so the UTC offset is right from the start.
  setenv("TZ", TIME_ZONE, 1);
  tzset();
  if (!uiBegin(display)) {
    Serial.println("UI buffer allocation failed");
    while (true) delay(1000);
  }
  logoBuffer = (uint16_t *)ps_malloc(LOGO_BYTES);
  if (logoBuffer == nullptr) logoBuffer = (uint16_t *)malloc(LOGO_BYTES);

  loadSettings();
  loadColors();
  loadStations();
  for (uint8_t kind = 0; kind < WEATHER_ICON_KINDS; ++kind) radioWeatherIconChanged(kind);
  updateTopBar();
  showSelectedStation();
  Serial.printf("Display ready: %d x %d\n", display->width(), display->height());

  setupEncoders();
  Audio::audio_info_callback = audioInfoCallback;
  audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
  audio.setVolumeSteps(VOLUME_STEPS);
  audio.setVolume(volumeLevel);
  audio.setConnectionTimeout(5000, 10000);
  audio.settings.SPECTRUM = true;  // 15-band analyzer data (evt_spectrum, every 50 ms)
  audioConfigured = true;

  WiFi.mode(WIFI_STA);
  WiFi.setHostname(HOSTNAME);
  WiFi.setSleep(false);  // no modem-sleep current pulses (screen backlight pulsing)
  WiFi.setAutoReconnect(true);
  webBegin();
  weatherBegin();
  updateWifi();
}

void updateWeather() {
  WeatherReading reading;
  if (weatherTake(reading)) {
    uiSetWeather(reading.valid, reading.kind, reading.temperatureTenths, reading.place);
  }
}

void loop() {
  audio.loop();
  webLoop();
  if (pendingPlayIndex >= 0 && !updating) {
    const int index = pendingPlayIndex;
    pendingPlayIndex = -1;
    encoderConnectPending = false;  // the web panel choice wins
    pendingTempPlay = false;
    selectStationIndex((size_t)index);
  }
  if (pendingTempPlay && !updating) {
    pendingTempPlay = false;
    encoderConnectPending = false;
    tempActive = true;
    connectSelectedStation();
  }
  updateWifi();
  checkStreamHealth();
  updateClockAndDate();
  handleEncoders();
  applyPendingMetadata();
  updateTopBar();
  updateWeather();
  saveSettingsIfNeeded();
  uiLoop();
  delay(1);
}
