#include "StationSearch.h"

#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include "StationStore.h"  // jsonEscape, STATION_URL_MAX
#include "TlsTrust.h"

// radio-browser.info: "all" resolves to one of the community servers; the others are fallbacks.
static const char *const SEARCH_SERVERS[] = {"all.api.radio-browser.info", "de1.api.radio-browser.info",
                                             "fi1.api.radio-browser.info"};
static const uint32_t SEARCH_TIMEOUT_MS = 10000;
static const size_t RESPONSE_MAX = 96 * 1024;       // PSRAM, only while the answer is read
static const uint32_t MIN_FREE_HEAP = 40 * 1024;    // TLS needs internal RAM

enum SearchState : uint8_t { SEARCH_IDLE, SEARCH_BUSY, SEARCH_DONE, SEARCH_FAILED };

static portMUX_TYPE searchMux = portMUX_INITIALIZER_UNLOCKED;
static volatile uint8_t state = SEARCH_IDLE;
static char searchQuery[64] = "";
static char searchError[96] = "";
static SearchResult *results = nullptr;  // SEARCH_MAX_RESULTS entries in PSRAM
static uint8_t resultCount = 0;

// ---------------------------------------------------------------------------
// The answer is written into a PSRAM buffer (HTTPClient handles chunked transfer).
class BufferSink : public Stream {
 public:
  BufferSink(char *buffer, size_t capacity) : buf(buffer), cap(capacity) {}
  size_t write(uint8_t c) override { return write(&c, 1); }
  size_t write(const uint8_t *data, size_t size) override {
    const size_t room = cap - 1 - len;  // keep one byte for the terminator
    const size_t n = size < room ? size : room;
    memcpy(buf + len, data, n);
    len += n;
    buf[len] = '\0';
    if (n < size) overflow = true;
    return size;  // pretend everything was taken; complete objects are parsed later
  }
  int available() override { return 0; }
  int read() override { return -1; }
  int peek() override { return -1; }
  void flush() override {}
  size_t length() const { return len; }
  bool overflow = false;

 private:
  char *buf;
  size_t cap;
  size_t len = 0;
};

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

// ---------------------------------------------------------------------------
// Minimal JSON reading inside one object [begin, end)
// ---------------------------------------------------------------------------
static const char *findKey(const char *begin, const char *end, const char *key) {
  const size_t keyLen = strlen(key);
  for (const char *p = begin; p + keyLen + 3 < end; ++p) {
    if (p[0] != '"' || strncmp(p + 1, key, keyLen) != 0 || p[1 + keyLen] != '"') continue;
    const char *v = p + keyLen + 2;
    while (v < end && (*v == ' ' || *v == '\t')) ++v;
    if (v >= end || *v != ':') continue;
    ++v;
    while (v < end && (*v == ' ' || *v == '\t' || *v == '\r' || *v == '\n')) ++v;
    return v < end ? v : nullptr;
  }
  return nullptr;
}

static size_t putUtf8(char *out, size_t pos, size_t size, uint32_t cp) {
  char tmp[4];
  size_t n;
  if (cp < 0x80) {
    tmp[0] = (char)cp;
    n = 1;
  } else if (cp < 0x800) {
    tmp[0] = (char)(0xC0 | (cp >> 6));
    tmp[1] = (char)(0x80 | (cp & 0x3F));
    n = 2;
  } else if (cp < 0x10000) {
    tmp[0] = (char)(0xE0 | (cp >> 12));
    tmp[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
    tmp[2] = (char)(0x80 | (cp & 0x3F));
    n = 3;
  } else {
    tmp[0] = (char)(0xF0 | (cp >> 18));
    tmp[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
    tmp[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
    tmp[3] = (char)(0x80 | (cp & 0x3F));
    n = 4;
  }
  if (pos + n >= size) return pos;  // does not fit: stop before this character
  memcpy(out + pos, tmp, n);
  return pos + n;
}

static uint32_t hex4(const char *p) {
  uint32_t v = 0;
  for (uint8_t i = 0; i < 4; ++i) {
    const char c = p[i];
    v <<= 4;
    if (c >= '0' && c <= '9') v |= c - '0';
    else if (c >= 'a' && c <= 'f') v |= c - 'a' + 10;
    else if (c >= 'A' && c <= 'F') v |= c - 'A' + 10;
    else return 0xFFFFFFFF;
  }
  return v;
}

// Reads a JSON string value (escapes decoded, UTF-8 kept, cut on a character boundary).
static void readString(const char *begin, const char *end, const char *key, char *out, size_t size) {
  out[0] = '\0';
  const char *p = findKey(begin, end, key);
  if (p == nullptr || *p != '"') return;
  ++p;
  size_t n = 0;
  while (p < end && *p != '"') {
    uint32_t cp = (uint8_t)*p;
    if (*p == '\\' && p + 1 < end) {
      ++p;
      switch (*p) {
        case 'n': case 'r': case 't': cp = ' '; break;
        case 'b': case 'f': cp = ' '; break;
        case 'u':
          if (p + 4 < end && (cp = hex4(p + 1)) != 0xFFFFFFFF) {
            p += 4;
            if (cp >= 0xD800 && cp <= 0xDBFF && p + 6 < end && p[1] == '\\' && p[2] == 'u') {
              const uint32_t low = hex4(p + 3);
              if (low >= 0xDC00 && low <= 0xDFFF) {
                cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                p += 6;
              }
            }
            if (cp >= 0xD800 && cp <= 0xDFFF) cp = '?';
          } else {
            cp = '?';
          }
          break;
        default: cp = (uint8_t)*p; break;  // \" \\ \/
      }
      if (cp < 0x20) cp = ' ';
      const size_t before = n;
      n = putUtf8(out, n, size, cp);
      if (n == before) break;
      ++p;
      continue;
    }
    // Raw UTF-8: copy the whole character or stop.
    size_t len = 1;
    if ((cp & 0xE0) == 0xC0) len = 2;
    else if ((cp & 0xF0) == 0xE0) len = 3;
    else if ((cp & 0xF8) == 0xF0) len = 4;
    if (p + len > end || n + len >= size) break;
    if (cp < 0x20) {
      out[n++] = ' ';
    } else {
      memcpy(out + n, p, len);
      n += len;
    }
    p += len;
  }
  out[n] = '\0';
  // trim spaces
  while (n > 0 && out[n - 1] == ' ') out[--n] = '\0';
  size_t start = 0;
  while (out[start] == ' ') ++start;
  if (start) memmove(out, out + start, n - start + 1);
}

static long readNumber(const char *begin, const char *end, const char *key) {
  const char *p = findKey(begin, end, key);
  if (p == nullptr) return 0;
  if (*p == '"') ++p;  // some servers send numbers as strings
  return strtol(p, nullptr, 10);
}

// Codecs decoded by ESP32-audioI2S; HLS (m3u8) playlists are not used.
static void checkPlayable(SearchResult &r, bool hls) {
  r.playable = false;
  char codec[sizeof(r.codec)];
  size_t i = 0;
  for (; r.codec[i] && i + 1 < sizeof(codec); ++i) codec[i] = (char)toupper((uint8_t)r.codec[i]);
  codec[i] = '\0';
  strlcpy(r.codec, codec, sizeof(r.codec));
  if (r.url[0] == '\0') {
    strlcpy(r.reason, "Brak adresu strumienia", sizeof(r.reason));
    return;
  }
  if (strncmp(r.url, "http://", 7) != 0 && strncmp(r.url, "https://", 8) != 0) {
    strlcpy(r.reason, "Nieobsługiwany adres", sizeof(r.reason));
    return;
  }
  if (strlen(r.url) > STATION_URL_MAX - 1) {
    strlcpy(r.reason, "Adres strumienia za długi", sizeof(r.reason));
    return;
  }
  if (hls || strstr(r.url, ".m3u8") != nullptr) {
    strlcpy(r.reason, "Format HLS nieobsługiwany", sizeof(r.reason));
    return;
  }
  static const char *const SUPPORTED[] = {"MP3", "AAC", "AAC+", "OGG", "OPUS", "VORBIS", "FLAC"};
  for (const char *s : SUPPORTED) {
    if (strcmp(codec, s) == 0) {
      r.playable = true;
      r.reason[0] = '\0';
      return;
    }
  }
  if (codec[0] == '\0' || strcmp(codec, "UNKNOWN") == 0) strlcpy(r.reason, "Nieznany format", sizeof(r.reason));
  else snprintf(r.reason, sizeof(r.reason), "Format %s nieobsługiwany", codec);
}

// Splits the top-level array into objects and fills results; returns the number found.
static uint8_t parseResults(const char *json, size_t length) {
  uint8_t count = 0;
  int depth = 0;
  bool inString = false, escape = false;
  const char *objectStart = nullptr;
  for (size_t i = 0; i < length && count < SEARCH_MAX_RESULTS; ++i) {
    const char c = json[i];
    if (inString) {
      if (escape) escape = false;
      else if (c == '\\') escape = true;
      else if (c == '"') inString = false;
      continue;
    }
    if (c == '"') {
      inString = true;
    } else if (c == '{' || c == '[') {
      if (c == '{' && depth == 1) objectStart = json + i;
      ++depth;
    } else if (c == '}' || c == ']') {
      --depth;
      if (c == '}' && depth == 1 && objectStart != nullptr) {
        const char *end = json + i + 1;
        SearchResult &r = results[count];
        readString(objectStart, end, "name", r.name, sizeof(r.name));
        readString(objectStart, end, "url_resolved", r.url, sizeof(r.url));
        if (r.url[0] == '\0') readString(objectStart, end, "url", r.url, sizeof(r.url));
        readString(objectStart, end, "country", r.country, sizeof(r.country));
        readString(objectStart, end, "language", r.language, sizeof(r.language));
        readString(objectStart, end, "tags", r.tags, sizeof(r.tags));
        readString(objectStart, end, "codec", r.codec, sizeof(r.codec));
        const long bitrate = readNumber(objectStart, end, "bitrate");
        r.bitrate = bitrate > 0 && bitrate < 10000 ? (uint16_t)bitrate : 0;
        checkPlayable(r, readNumber(objectStart, end, "hls") != 0);
        if (r.name[0] == '\0') strlcpy(r.name, "(bez nazwy)", sizeof(r.name));
        ++count;
        objectStart = nullptr;
      }
    }
  }
  return count;
}

// ---------------------------------------------------------------------------
static void finish(uint8_t newState, uint8_t count, const char *error) {
  portENTER_CRITICAL(&searchMux);
  resultCount = count;
  strlcpy(searchError, error ? error : "", sizeof(searchError));
  state = newState;
  portEXIT_CRITICAL(&searchMux);
}

static void searchTask(void *) {
  char query[64];
  portENTER_CRITICAL(&searchMux);
  strlcpy(query, searchQuery, sizeof(query));
  portEXIT_CRITICAL(&searchMux);

  char *buffer = (char *)ps_malloc(RESPONSE_MAX);
  if (buffer == nullptr) {
    finish(SEARCH_FAILED, 0, "Za mało pamięci PSRAM");
    vTaskDelete(nullptr);
    return;
  }
  const String path = "/json/stations/search?name=" + urlEncode(query) + "&limit=" +
                      String(SEARCH_MAX_RESULTS) + "&hidebroken=true&order=clickcount&reverse=true";
  String error = "Serwis wyszukiwania niedostępny";
  bool received = false;
  size_t length = 0;
  for (const char *server : SEARCH_SERVERS) {
    if (WiFi.status() != WL_CONNECTED) {
      error = "Brak połączenia z internetem";
      break;
    }
    WiFiClientSecure client;
    tlsUseTrustedRoots(client);  // server certificate verified (built-in root CA bundle)
    HTTPClient http;
    http.setConnectTimeout(SEARCH_TIMEOUT_MS);
    http.setTimeout(SEARCH_TIMEOUT_MS);
    http.setUserAgent("RadioWilma/1.0 (ESP32-S3)");
    if (!http.begin(client, String("https://") + server + path)) continue;
    const int code = http.GET();
    if (code < 0) tlsLogError(client, "Station search");
    String tlsError;
    if (code == 200 && !tlsCertificateDatesOk(client, "Station search", tlsError)) {
      http.end();  // answer discarded, next server
      continue;
    }
    if (code == 200) {
      BufferSink sink(buffer, RESPONSE_MAX);
      const int written = http.writeToStream(&sink);
      length = sink.length();
      received = written > 0 || length > 0;
      if (sink.overflow) Serial.println("Station search: answer cut to the buffer size");
    } else {
      Serial.printf("Station search: %s HTTP %d\n", server, code);
    }
    http.end();
    if (received) break;
  }

  if (!received) {
    finish(SEARCH_FAILED, 0, error.c_str());
  } else {
    const uint8_t count = parseResults(buffer, length);
    finish(SEARCH_DONE, count, count ? "" : "Brak wyników");
  }
  free(buffer);
  vTaskDelete(nullptr);
}

bool searchStart(const String &query, String &error) {
  String text = query;
  text.trim();
  if (text.isEmpty()) {
    error = "Wpisz nazwę stacji";
    return false;
  }
  if (state == SEARCH_BUSY) {
    error = "Trwa poprzednie wyszukiwanie";
    return false;
  }
  if (WiFi.status() != WL_CONNECTED) {
    error = "Brak połączenia z internetem";
    return false;
  }
  if (ESP.getMaxAllocHeap() < MIN_FREE_HEAP) {
    error = "Za mało wolnej pamięci RAM, spróbuj za chwilę";
    return false;
  }
  if (results == nullptr) {
    results = (SearchResult *)ps_malloc(sizeof(SearchResult) * SEARCH_MAX_RESULTS);
    if (results == nullptr) {
      error = "Za mało pamięci PSRAM";
      return false;
    }
  }
  portENTER_CRITICAL(&searchMux);
  strlcpy(searchQuery, text.c_str(), sizeof(searchQuery));
  searchError[0] = '\0';
  resultCount = 0;
  state = SEARCH_BUSY;
  portEXIT_CRITICAL(&searchMux);
  // Low priority, core 1 (audio decoding runs on core 0); the task ends itself.
  if (xTaskCreatePinnedToCore(searchTask, "search", 12288, nullptr, 1, nullptr, 1) != pdPASS) {
    finish(SEARCH_FAILED, 0, "Nie można uruchomić wyszukiwania");
    error = "Za mało pamięci RAM";
    return false;
  }
  return true;
}

String searchStatusJson() {
  static const char *const NAMES[] = {"idle", "busy", "done", "error"};
  uint8_t current, count;
  char query[64], error[96];
  portENTER_CRITICAL(&searchMux);
  current = state;
  count = resultCount;
  strlcpy(query, searchQuery, sizeof(query));
  strlcpy(error, searchError, sizeof(error));
  portEXIT_CRITICAL(&searchMux);
  String json = "{\"state\":\"" + String(NAMES[current]) + "\",\"query\":\"" + jsonEscape(query) +
                "\",\"error\":\"" + jsonEscape(error) + "\",\"results\":[";
  if (current == SEARCH_DONE && results != nullptr) {
    json.reserve(json.length() + count * 230);
    for (uint8_t i = 0; i < count; ++i) {
      const SearchResult &r = results[i];
      if (i) json += ',';
      json += "{\"i\":" + String(i) + ",\"name\":\"" + jsonEscape(r.name) + "\",\"country\":\"" +
              jsonEscape(r.country) + "\",\"language\":\"" + jsonEscape(r.language) + "\",\"tags\":\"" +
              jsonEscape(r.tags) + "\",\"codec\":\"" + jsonEscape(r.codec) + "\",\"bitrate\":" +
              String(r.bitrate) + ",\"playable\":" + (r.playable ? "true" : "false") + ",\"reason\":\"" +
              jsonEscape(r.reason) + "\"}";
    }
  }
  json += "]}";
  return json;
}

bool searchGetResult(uint8_t index, SearchResult &out) {
  if (state != SEARCH_DONE || results == nullptr || index >= resultCount) return false;
  out = results[index];
  return true;
}
