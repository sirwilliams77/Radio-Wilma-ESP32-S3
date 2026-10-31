#pragma once
// Current weather from Open-Meteo (free for non-commercial use, no API key).
// Runs in its own FreeRTOS task: geocoding of the place name once, then the
// current conditions every 15 minutes while Wi-Fi is connected.
#include <Arduino.h>

struct WeatherReading {
  bool valid;
  uint8_t kind;             // UiWeatherKind
  int16_t temperatureTenths;
  char place[48];
};

void weatherBegin();                          // loads the saved place, starts the task
void weatherSetPlace(const String &query);    // "" disables the weather
bool weatherTake(WeatherReading &reading);    // true when something changed since the last call
String weatherStatusJson();                   // for the web panel
