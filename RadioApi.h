#pragma once
// Functions of the radio (implemented in Radio_Wilma.ino) used by the web panel.
// All of them are called from loop() context (WebServer::handleClient), never from ISRs.
#include <Arduino.h>
#include <functional>
#include <vector>
#include "StationStore.h"

// Playback
String radioStatusJson();
void radioPlayIndex(int index);  // deferred: connects on the next loop()
void radioStep(int direction);   // previous (-1) / next (+1) station
void radioTogglePause();
void radioSetVolume(int volume);
void radioToggleMute();

// Station list
const std::vector<Station> &radioStations();
bool radioReplaceStations(const std::vector<Station> &stations, String &error);
void radioLogoChanged(uint32_t stationId);
void radioWeatherIconChanged(uint8_t kind);

// Internet station search (StationSearch): index into the last finished search.
bool radioPlaySearchResult(uint8_t index, String &error);  // plays without changing the favourites
bool radioAddSearchResult(uint8_t index, String &error);   // appends to the favourites  // reloads an own weather icon (or the built-in one)

// Settings
String radioSettingsJson();
void radioSetWifi(const String &ssid, const String &password);
void radioSetPanelPassword(const String &password);
const String &radioPanelPassword();

// Screen colours: JSON {"colors":{key:"#rrggbb"},"defaults":{...}}; value(key) gives a request field.
String radioColorsJson();
bool radioSetColors(const std::function<String(const char *)> &value, bool reset);

// Firmware update progress (screen message, audio stopped while flashing)
void radioUpdateStarted();
void radioUpdateProgress(size_t written, size_t total);
void radioUpdateFailed(const String &reason);
