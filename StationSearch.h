#pragma once
// Internet station search (radio-browser.info, free community database, no API key).
// The search runs in a short-lived FreeRTOS task, so playback and the web panel keep
// working; the answer is read into PSRAM and only the fields shown in the panel are kept.
#include <Arduino.h>

static const uint8_t SEARCH_MAX_RESULTS = 25;

struct SearchResult {
  char name[64];
  char url[256];       // url_resolved (playlists already resolved by the database)
  char country[40];
  char language[40];
  char tags[64];       // genres, comma separated
  char codec[12];
  uint16_t bitrate;    // kbps, 0 = unknown
  bool playable;       // codec and address supported by the audio library
  char reason[40];     // why not playable ("" when playable)
};

// Starts a search for a station name. false (with error) when busy, offline or empty.
bool searchStart(const String &query, String &error);
// {"state":"idle|busy|done|error","query":"...","error":"...","results":[...]}
String searchStatusJson();
// Copies one result of the last finished search. false when there is no such result.
bool searchGetResult(uint8_t index, SearchResult &out);
