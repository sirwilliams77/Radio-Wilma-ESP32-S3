#include "Weather.h"

#include <HTTPClient.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include "RadioUi.h"
#include "StationStore.h"  // jsonEscape

static const uint32_t REFRESH_MS = 15UL * 60UL * 1000UL;
static const uint32_t RETRY_MS = 2UL * 60UL * 1000UL;
static const uint32_t HTTP_TIMEOUT_MS = 8000;

static portMUX_TYPE weatherMux = portMUX_INITIALIZER_UNLOCKED;
static Preferences weatherPrefs;

// Shared between the weather task and loop() (guarded by weatherMux).
static char placeQuery[64] = "";   // what the user typed
static char placeName[48] = "";    // name returned by geocoding
static float latitude = 0, longitude = 0;
static bool haveLocation = false;
static volatile bool geocodeRequested = false;
static volatile bool fetchRequested = false;
static WeatherReading current = {false, UI_WX_CLOUDY, 0, ""};
static int weatherCode = -1;
static uint32_t lastSuccessAt = 0;
static bool changed = true;
static char lastError[96] = "";

// ---------------------------------------------------------------------------
static String urlEncode(const char *text) {
  static const char *hex = "0123456789ABCDEF";
  String out;
  for (const uint8_t *p = (const uint8_t *)text; *p; ++p) {
    if (isalnum(*p) || *p == '-' || *p == '_' || *p == '.' || *p == '~') {
      out += (char)*p;
    } else {
      out += '%';
      out += hex[*p >> 4];
      out += hex[*p & 15];
    }
  }
  return out;
}

static bool httpGet(const String &url, String &body, String &error) {
  WiFiClientSecure client;
  client.setInsecure();  // public weather data; certificate is not verified
  HTTPClient http;
  http.setConnectTimeout(HTTP_TIMEOUT_MS);
  http.setTimeout(HTTP_TIMEOUT_MS);
  if (!http.begin(client, url)) {
    error = "Nie można połączyć z Open-Meteo";
    return false;
  }
  const int code = http.GET();
  if (code == 200) body = http.getString();
  else error = "Open-Meteo: błąd HTTP " + String(code);
  http.end();
  return code == 200;
}

// Finds "key": after position from and returns the index of the value (or -1).
static int findValue(const String &json, const char *key, int from) {
  const String pattern = String("\"") + key + "\":";
  const int at = json.indexOf(pattern, from);
  return at < 0 ? -1 : at + pattern.length();
}

static bool readNumber(const String &json, const char *key, int from, float &value) {
  const int at = findValue(json, key, from);
  if (at < 0) return false;
  value = strtof(json.c_str() + at, nullptr);
  return true;
}

static bool readString(const String &json, const char *key, int from, char *out, size_t size) {
  int at = findValue(json, key, from);
  if (at < 0 || json[at] != '"') return false;
  ++at;
  size_t n = 0;
  while (at < (int)json.length() && json[at] != '"' && n + 1 < size) {
    if (json[at] == '\\' && at + 1 < (int)json.length()) ++at;
    out[n++] = json[at++];
  }
  // never leave half of a UTF-8 character at the end
  while (n > 0 && ((uint8_t)out[n - 1] & 0xC0) == 0x80) --n;
  if (n > 0 && ((uint8_t)out[n - 1] & 0xC0) == 0xC0) --n;
  out[n] = '\0';
  return true;
}

static uint8_t kindFromCode(int code, bool day) {
  if (code == 0) return day ? UI_WX_CLEAR_DAY : UI_WX_CLEAR_NIGHT;
  if (code == 1 || code == 2) return day ? UI_WX_PARTLY_DAY : UI_WX_PARTLY_NIGHT;
  if (code == 3) return UI_WX_CLOUDY;
  if (code == 45 || code == 48) return UI_WX_FOG;
  if (code >= 51 && code <= 57) return UI_WX_DRIZZLE;
  if ((code >= 61 && code <= 67) || (code >= 80 && code <= 82)) return UI_WX_RAIN;
  if ((code >= 71 && code <= 77) || code == 85 || code == 86) return UI_WX_SNOW;
  if (code >= 95) return UI_WX_STORM;
  return UI_WX_CLOUDY;
}

static void setError(const String &error) {
  portENTER_CRITICAL(&weatherMux);
  strlcpy(lastError, error.c_str(), sizeof(lastError));
  portEXIT_CRITICAL(&weatherMux);
  Serial.printf("Weather: %s\n", error.c_str());
}

// 1 = found, 0 = place not found, -1 = network/server error (retried later)
static int geocode() {
  char query[64];
  portENTER_CRITICAL(&weatherMux);
  strlcpy(query, placeQuery, sizeof(query));
  portEXIT_CRITICAL(&weatherMux);
  String body, error;
  const String url = "https://geocoding-api.open-meteo.com/v1/search?count=1&language=pl&format=json&name=" +
                     urlEncode(query);
  if (!httpGet(url, body, error)) {
    setError(error);
    return -1;
  }
  const int results = body.indexOf("\"results\"");
  float lat = 0, lon = 0;
  char name[48] = "";
  if (results < 0 || !readNumber(body, "latitude", results, lat) ||
      !readNumber(body, "longitude", results, lon) ||
      !readString(body, "name", results, name, sizeof(name))) {
    setError(String("Nie znaleziono miejscowości \"") + query + "\"");
    portENTER_CRITICAL(&weatherMux);
    haveLocation = false;
    current.valid = false;
    changed = true;
    portEXIT_CRITICAL(&weatherMux);
    return 0;
  }
  portENTER_CRITICAL(&weatherMux);
  latitude = lat;
  longitude = lon;
  strlcpy(placeName, name, sizeof(placeName));
  haveLocation = true;
  portEXIT_CRITICAL(&weatherMux);
  weatherPrefs.putString("name", name);
  weatherPrefs.putFloat("lat", lat);
  weatherPrefs.putFloat("lon", lon);
  return 1;
}

static bool fetchCurrent() {
  float lat, lon;
  portENTER_CRITICAL(&weatherMux);
  lat = latitude;
  lon = longitude;
  portEXIT_CRITICAL(&weatherMux);
  char url[200];
  snprintf(url, sizeof(url),
           "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f"
           "&current=temperature_2m,weather_code,is_day",
           lat, lon);
  String body, error;
  if (!httpGet(url, body, error)) {
    setError(error);
    return false;
  }
  const int at = body.indexOf("\"current\":{");  // skips "current_units"
  float temperature = 0, code = -1, isDay = 1;
  if (at < 0 || !readNumber(body, "temperature_2m", at, temperature) ||
      !readNumber(body, "weather_code", at, code)) {
    setError("Nieoczekiwana odpowiedź Open-Meteo");
    return false;
  }
  readNumber(body, "is_day", at, isDay);
  portENTER_CRITICAL(&weatherMux);
  current.valid = true;
  current.kind = kindFromCode((int)code, isDay > 0.5f);
  current.temperatureTenths = (int16_t)lroundf(temperature * 10.0f);
  strlcpy(current.place, placeName, sizeof(current.place));
  weatherCode = (int)code;
  lastSuccessAt = millis();
  lastError[0] = '\0';
  changed = true;
  portEXIT_CRITICAL(&weatherMux);
  return true;
}

static void weatherTask(void *) {
  uint32_t nextFetchAt = 0;
  uint32_t nextGeocodeAt = 0;
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(1000));
    if (WiFi.status() != WL_CONNECTED || time(nullptr) < 1700000000) continue;  // TLS needs time
    if (geocodeRequested && (int32_t)(millis() - nextGeocodeAt) >= 0) {
      const int result = geocode();
      if (result < 0) {
        nextGeocodeAt = millis() + RETRY_MS;  // keep the request, try again later
      } else {
        geocodeRequested = false;
        if (result > 0) fetchRequested = true;
      }
    }
    if (!haveLocation) continue;
    if (fetchRequested || (int32_t)(millis() - nextFetchAt) >= 0) {
      fetchRequested = false;
      nextFetchAt = millis() + (fetchCurrent() ? REFRESH_MS : RETRY_MS);
    }
  }
}

// ---------------------------------------------------------------------------
void weatherBegin() {
  weatherPrefs.begin("weather", false);
  strlcpy(placeQuery, weatherPrefs.getString("query", "").c_str(), sizeof(placeQuery));
  strlcpy(placeName, weatherPrefs.getString("name", "").c_str(), sizeof(placeName));
  latitude = weatherPrefs.getFloat("lat", 0);
  longitude = weatherPrefs.getFloat("lon", 0);
  haveLocation = placeQuery[0] != '\0' && placeName[0] != '\0';
  if (placeQuery[0] != '\0' && !haveLocation) geocodeRequested = true;
  fetchRequested = haveLocation;
  xTaskCreatePinnedToCore(weatherTask, "weather", 12288, nullptr, 1, nullptr, 1);
}

void weatherSetPlace(const String &query) {
  String trimmed = query;
  trimmed.trim();
  weatherPrefs.putString("query", trimmed);
  portENTER_CRITICAL(&weatherMux);
  strlcpy(placeQuery, trimmed.c_str(), sizeof(placeQuery));
  haveLocation = false;
  placeName[0] = '\0';
  current.valid = false;
  lastError[0] = '\0';
  changed = true;
  portEXIT_CRITICAL(&weatherMux);
  if (!trimmed.isEmpty()) geocodeRequested = true;
}

bool weatherTake(WeatherReading &reading) {
  portENTER_CRITICAL(&weatherMux);
  const bool wasChanged = changed;
  changed = false;
  reading = current;
  portEXIT_CRITICAL(&weatherMux);
  return wasChanged;
}

String weatherStatusJson() {
  char query[64], name[48], error[96];
  WeatherReading reading;
  int code;
  uint32_t age;
  portENTER_CRITICAL(&weatherMux);
  strlcpy(query, placeQuery, sizeof(query));
  strlcpy(name, placeName, sizeof(name));
  strlcpy(error, lastError, sizeof(error));
  reading = current;
  code = weatherCode;
  age = lastSuccessAt ? (millis() - lastSuccessAt) / 60000UL : 0;
  portEXIT_CRITICAL(&weatherMux);
  String json = "{\"query\":\"" + jsonEscape(query) + "\"";
  json += ",\"place\":\"" + jsonEscape(name) + "\"";
  json += ",\"valid\":" + String(reading.valid ? "true" : "false");
  json += ",\"temp\":" + String(reading.temperatureTenths / 10.0f, 1);
  json += ",\"code\":" + String(code);
  json += ",\"ageMin\":" + String(age);
  json += ",\"error\":\"" + jsonEscape(error) + "\"}";
  return json;
}
