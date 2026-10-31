#pragma once
// Station list and station logos stored on the data partition of the flash
// (FAT from "16M Flash (3MB APP/9.9MB FATFS)", or a SPIFFS-type partition as LittleFS).
//   /stations.jsn      JSON array: [{"id":1,"name":"...","url":"...","desc":"..."}]
//   /logos/<id>.565    raw RGB565 logo, LOGO_W x LOGO_H, little-endian (11616 bytes)
#include <Arduino.h>
#include <vector>
#include "UiTypes.h"

struct Station {
  uint32_t id;
  String name;
  String url;
  String desc;
};

static const size_t MAX_STATIONS = 100;
static const size_t STATION_NAME_MAX = 63;   // bytes (UTF-8)
static const size_t STATION_URL_MAX = 255;
static const size_t STATION_DESC_MAX = 95;
static const size_t LOGO_BYTES = (size_t)LOGO_W * LOGO_H * 2;

#include <FS.h>

bool storeBegin();   // mounts the FAT (or SPIFFS-type) partition, formats it when empty
bool storeReady();
const String &storeLastError();    // human-readable reason of the last failure (Polish)
const String &storeDescription();  // e.g. "FAT (ffat)"
uint32_t storeUsedBytes();
uint32_t storeTotalBytes();
fs::File storeOpen(const String &path, const char *mode);
bool storeLoad(std::vector<Station> &stations);
bool storeSave(const std::vector<Station> &stations);

// JSON helpers (list exchanged with the web panel and used for export/import).
String jsonEscape(const String &text);
String stationsToJson(const std::vector<Station> &stations, bool withLogoFlags);
// Parses and validates a list. Stations without id (or with a duplicate) get a new id.
bool stationsFromJson(const String &json, std::vector<Station> &out, String &error);

String logoPath(uint32_t id);
bool logoExists(uint32_t id);
bool logoLoad(uint32_t id, uint16_t *buffer);  // buffer: LOGO_W * LOGO_H pixels
bool logoSave(uint32_t id, const uint8_t *data, size_t length);
void logoRemove(uint32_t id);
void logoCleanup(const std::vector<Station> &stations);  // deletes logos of removed stations

// Weather icons replaced in the web panel: /wx/<kind>.565, raw RGB565 78x60, little-endian,
// already composed on the black screen background. kind = UiWeatherKind (0..9).
static const int16_t WEATHER_ICON_W = 78, WEATHER_ICON_H = 60;
static const size_t WEATHER_ICON_BYTES = (size_t)WEATHER_ICON_W * WEATHER_ICON_H * 2;
static const uint8_t WEATHER_ICON_KINDS = 10;
bool weatherIconLoad(uint8_t kind, uint16_t *buffer);  // false = no own icon (use the built-in one)
bool weatherIconSave(uint8_t kind, const uint8_t *data, size_t length);
void weatherIconRemove(uint8_t kind);
