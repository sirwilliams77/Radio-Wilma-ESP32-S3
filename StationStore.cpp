#include "StationStore.h"

#include <FFat.h>
#include <LittleFS.h>
#include <esp_partition.h>

static const char *STATIONS_FILE = "/stations.jsn";
static const char *STATIONS_TMP = "/stations.tmp";
static const char *LOGO_DIR = "/logos";
static const char *WEATHER_ICON_DIR = "/wx";
static bool mounted = false;
static fs::FS *fsys = nullptr;
static String fsDescription = "brak";
static String lastError;

// Uses whichever data partition the selected partition scheme provides:
// FAT ("16M Flash (3MB APP/9.9MB FATFS)") or SPIFFS-type (mounted as LittleFS).
bool storeBegin() {
  mounted = false;
  const esp_partition_t *fat = esp_partition_find_first(
      ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_FAT, nullptr);
  if (fat != nullptr) {
    if (FFat.begin(true, "/ffat", 10, fat->label)) {
      fsys = &FFat;
      mounted = true;
      fsDescription = String("FAT (") + fat->label + ")";
    } else {
      lastError = String("Nie można zamontować partycji FAT \"") + fat->label + "\"";
    }
  }
  if (!mounted) {
    const esp_partition_t *spiffs = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_SPIFFS, nullptr);
    if (spiffs != nullptr) {
      if (LittleFS.begin(true, "/littlefs", 10, spiffs->label)) {
        fsys = &LittleFS;
        mounted = true;
        fsDescription = String("LittleFS (") + spiffs->label + ")";
      } else {
        lastError = String("Nie można zamontować partycji \"") + spiffs->label + "\"";
      }
    }
  }
  if (!mounted && fat == nullptr) {
    lastError = "Brak partycji na pliki. W Arduino IDE wybierz Partition Scheme: "
                "16M Flash (3MB APP/9.9MB FATFS)";
  }
  if (mounted && !fsys->exists(LOGO_DIR)) fsys->mkdir(LOGO_DIR);
  if (mounted && !fsys->exists(WEATHER_ICON_DIR)) fsys->mkdir(WEATHER_ICON_DIR);
  Serial.printf("Storage: %s%s%s\n", mounted ? fsDescription.c_str() : "NOT AVAILABLE - ",
                mounted ? "" : lastError.c_str(), "");
  return mounted;
}

bool storeReady() { return mounted; }
const String &storeLastError() { return lastError; }
const String &storeDescription() { return fsDescription; }

uint32_t storeUsedBytes() {
  if (!mounted) return 0;
  return fsys == &FFat ? (uint32_t)FFat.usedBytes() : (uint32_t)LittleFS.usedBytes();
}

uint32_t storeTotalBytes() {
  if (!mounted) return 0;
  return fsys == &FFat ? (uint32_t)FFat.totalBytes() : (uint32_t)LittleFS.totalBytes();
}

File storeOpen(const String &path, const char *mode) {
  return mounted ? fsys->open(path, mode) : File();
}

// ---------------------------------------------------------------------------
// JSON
// ---------------------------------------------------------------------------
String jsonEscape(const String &text) {
  String out;
  out.reserve(text.length() + 8);
  for (size_t i = 0; i < text.length(); ++i) {
    const char c = text[i];
    switch (c) {
      case '"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default:
        if ((uint8_t)c < 0x20) {
          char buffer[8];
          snprintf(buffer, sizeof(buffer), "\\u%04x", (unsigned)(uint8_t)c);
          out += buffer;
        } else {
          out += c;
        }
    }
  }
  return out;
}

String stationsToJson(const std::vector<Station> &stations, bool withLogoFlags) {
  String json = "[";
  for (size_t i = 0; i < stations.size(); ++i) {
    const Station &s = stations[i];
    if (i) json += ',';
    json += "{\"id\":";
    json += String(s.id);
    json += ",\"name\":\"" + jsonEscape(s.name) + "\"";
    json += ",\"url\":\"" + jsonEscape(s.url) + "\"";
    json += ",\"desc\":\"" + jsonEscape(s.desc) + "\"";
    if (withLogoFlags) json += logoExists(s.id) ? ",\"logo\":true" : ",\"logo\":false";
    json += '}';
  }
  json += ']';
  return json;
}

namespace {

// Minimal reader for an array of flat objects (strings, numbers, true/false/null).
class JsonReader {
 public:
  explicit JsonReader(const char *text) : p(text) {}

  void skipWs() {
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') ++p;
  }
  bool consume(char c) {
    skipWs();
    if (*p != c) return false;
    ++p;
    return true;
  }
  char peek() {
    skipWs();
    return *p;
  }

  bool readString(String &out) {
    out = "";
    if (!consume('"')) return false;
    while (*p && *p != '"') {
      if (*p != '\\') {
        out += *p++;
        continue;
      }
      ++p;
      switch (*p) {
        case '"': out += '"'; break;
        case '\\': out += '\\'; break;
        case '/': out += '/'; break;
        case 'b': case 'f': break;
        case 'n': case 'r': case 't': out += ' '; break;
        case 'u': {
          uint32_t cp = 0;
          if (!readHex4(p + 1, cp)) return false;
          p += 4;
          if (cp >= 0xD800 && cp <= 0xDBFF && p[1] == '\\' && p[2] == 'u') {
            uint32_t low = 0;
            if (readHex4(p + 3, low) && low >= 0xDC00 && low <= 0xDFFF) {
              cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
              p += 6;
            }
          }
          appendUtf8(out, cp);
          break;
        }
        default: return false;
      }
      if (*p) ++p;
    }
    return consume('"');
  }

  bool readNumber(double &value) {
    skipWs();
    char *end = nullptr;
    value = strtod(p, &end);
    if (end == p) return false;
    p = end;
    return true;
  }

  bool skipValue() {
    const char c = peek();
    if (c == '"') {
      String ignored;
      return readString(ignored);
    }
    if (c == '-' || (c >= '0' && c <= '9')) {
      double ignored;
      return readNumber(ignored);
    }
    if (strncmp(p, "true", 4) == 0) { p += 4; return true; }
    if (strncmp(p, "false", 5) == 0) { p += 5; return true; }
    if (strncmp(p, "null", 4) == 0) { p += 4; return true; }
    return false;  // nested objects/arrays are not part of the format
  }

  bool atEnd() {
    skipWs();
    return *p == '\0';
  }

 private:
  const char *p;

  static bool readHex4(const char *s, uint32_t &value) {
    value = 0;
    for (int i = 0; i < 4; ++i) {
      const char c = s[i];
      value <<= 4;
      if (c >= '0' && c <= '9') value |= c - '0';
      else if (c >= 'a' && c <= 'f') value |= c - 'a' + 10;
      else if (c >= 'A' && c <= 'F') value |= c - 'A' + 10;
      else return false;
    }
    return true;
  }

  static void appendUtf8(String &out, uint32_t cp) {
    if (cp < 0x20) return;
    if (cp < 0x80) {
      out += (char)cp;
    } else if (cp < 0x800) {
      out += (char)(0xC0 | (cp >> 6));
      out += (char)(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
      out += (char)(0xE0 | (cp >> 12));
      out += (char)(0x80 | ((cp >> 6) & 0x3F));
      out += (char)(0x80 | (cp & 0x3F));
    } else {
      out += (char)(0xF0 | (cp >> 18));
      out += (char)(0x80 | ((cp >> 12) & 0x3F));
      out += (char)(0x80 | ((cp >> 6) & 0x3F));
      out += (char)(0x80 | (cp & 0x3F));
    }
  }
};

// Trims spaces and cuts to maxBytes without splitting a UTF-8 character.
String cleanText(String text, size_t maxBytes) {
  text.trim();
  if (text.length() <= maxBytes) return text;
  size_t cut = maxBytes;
  while (cut > 0 && ((uint8_t)text[cut] & 0xC0) == 0x80) --cut;
  return text.substring(0, cut);
}

}  // namespace

bool stationsFromJson(const String &json, std::vector<Station> &out, String &error) {
  out.clear();
  JsonReader reader(json.c_str());
  if (!reader.consume('[')) {
    error = "Oczekiwano tablicy JSON [...]";
    return false;
  }
  if (reader.peek() != ']') {
    do {
      if (!reader.consume('{')) {
        error = "Nieprawidłowy element listy";
        return false;
      }
      Station station = {0, "", "", ""};
      if (reader.peek() != '}') {
        do {
          String key;
          if (!reader.readString(key) || !reader.consume(':')) {
            error = "Nieprawidłowy klucz w JSON";
            return false;
          }
          bool ok;
          if (key == "id") {
            double value = 0;
            ok = reader.readNumber(value);
            if (ok && value > 0 && value < 4000000000.0) station.id = (uint32_t)value;
          } else if (key == "name") {
            ok = reader.readString(station.name);
          } else if (key == "url") {
            ok = reader.readString(station.url);
          } else if (key == "desc") {
            ok = reader.readString(station.desc);
          } else {
            ok = reader.skipValue();
          }
          if (!ok) {
            error = "Nieprawidłowa wartość pola \"" + key + "\"";
            return false;
          }
        } while (reader.consume(','));
      }
      if (!reader.consume('}')) {
        error = "Brak zamknięcia obiektu }";
        return false;
      }
      station.name = cleanText(station.name, STATION_NAME_MAX);
      station.url = cleanText(station.url, STATION_URL_MAX);
      station.desc = cleanText(station.desc, STATION_DESC_MAX);
      if (station.name.isEmpty()) {
        error = "Stacja nr " + String(out.size() + 1) + " nie ma nazwy";
        return false;
      }
      if (!station.url.startsWith("http://") && !station.url.startsWith("https://")) {
        error = "Adres stacji \"" + station.name + "\" musi zaczynać się od http:// lub https://";
        return false;
      }
      if (out.size() >= MAX_STATIONS) {
        error = "Maksymalnie " + String(MAX_STATIONS) + " stacji";
        return false;
      }
      out.push_back(station);
    } while (reader.consume(','));
  }
  if (!reader.consume(']') || !reader.atEnd()) {
    error = "Nieprawidłowy koniec JSON";
    return false;
  }

  // Unique ids: keep the first occurrence, give new ids to missing/duplicate ones.
  uint32_t maxId = 0;
  for (const Station &s : out) {
    if (s.id > maxId) maxId = s.id;
  }
  for (size_t i = 0; i < out.size(); ++i) {
    bool duplicate = false;
    for (size_t j = 0; j < i && !duplicate; ++j) duplicate = out[j].id == out[i].id;
    if (out[i].id == 0 || duplicate) out[i].id = ++maxId;
  }
  return true;
}

// ---------------------------------------------------------------------------
// Files
// ---------------------------------------------------------------------------
bool storeLoad(std::vector<Station> &stations) {
  if (!mounted || !fsys->exists(STATIONS_FILE)) return false;
  File file = fsys->open(STATIONS_FILE, "r");
  if (!file) return false;
  String json = file.readString();
  file.close();
  String error;
  if (!stationsFromJson(json, stations, error)) {
    Serial.printf("stations.jsn: %s\n", error.c_str());
    return false;
  }
  return true;
}

bool storeSave(const std::vector<Station> &stations) {
  if (!mounted) return false;  // lastError explains why the partition is missing
  File file = fsys->open(STATIONS_TMP, "w");
  if (!file) {
    lastError = "Nie można utworzyć pliku na partycji " + fsDescription;
    return false;
  }
  const String json = stationsToJson(stations, false);
  const bool written = file.print(json) == json.length();
  file.close();
  if (!written) {
    fsys->remove(STATIONS_TMP);
    lastError = "Brak miejsca lub błąd zapisu na partycji " + fsDescription;
    return false;
  }
  if (fsys->exists(STATIONS_FILE)) fsys->remove(STATIONS_FILE);
  if (!fsys->rename(STATIONS_TMP, STATIONS_FILE)) {
    lastError = "Nie można zamienić pliku listy stacji";
    return false;
  }
  return true;
}

String logoPath(uint32_t id) { return String(LOGO_DIR) + "/" + String(id) + ".565"; }

bool logoExists(uint32_t id) { return mounted && fsys->exists(logoPath(id)); }

bool logoLoad(uint32_t id, uint16_t *buffer) {
  if (!mounted || buffer == nullptr) return false;
  const String path = logoPath(id);
  if (!fsys->exists(path)) return false;
  File file = fsys->open(path, "r");
  if (!file) return false;
  const bool ok = file.size() == LOGO_BYTES && file.read((uint8_t *)buffer, LOGO_BYTES) == LOGO_BYTES;
  file.close();
  return ok;
}

bool logoSave(uint32_t id, const uint8_t *data, size_t length) {
  if (!mounted || length != LOGO_BYTES) return false;
  File file = fsys->open(logoPath(id), "w");
  if (!file) return false;
  const bool ok = file.write(data, length) == length;
  file.close();
  return ok;
}

static String weatherIconPath(uint8_t kind) {
  return String(WEATHER_ICON_DIR) + "/" + String(kind) + ".565";
}

bool weatherIconLoad(uint8_t kind, uint16_t *buffer) {
  if (!mounted || buffer == nullptr || kind >= WEATHER_ICON_KINDS) return false;
  const String path = weatherIconPath(kind);
  if (!fsys->exists(path)) return false;
  File file = fsys->open(path, "r");
  if (!file) return false;
  const bool ok = file.size() == WEATHER_ICON_BYTES &&
                  file.read((uint8_t *)buffer, WEATHER_ICON_BYTES) == WEATHER_ICON_BYTES;
  file.close();
  return ok;
}

bool weatherIconSave(uint8_t kind, const uint8_t *data, size_t length) {
  if (!mounted || kind >= WEATHER_ICON_KINDS || length != WEATHER_ICON_BYTES) return false;
  File file = fsys->open(weatherIconPath(kind), "w");
  if (!file) return false;
  const bool ok = file.write(data, length) == length;
  file.close();
  return ok;
}

void weatherIconRemove(uint8_t kind) {
  if (mounted && kind < WEATHER_ICON_KINDS) fsys->remove(weatherIconPath(kind));
}

void logoRemove(uint32_t id) {
  if (mounted) fsys->remove(logoPath(id));
}

void logoCleanup(const std::vector<Station> &stations) {
  if (!mounted) return;
  File dir = fsys->open(LOGO_DIR);
  if (!dir || !dir.isDirectory()) return;
  std::vector<String> orphans;
  for (File entry = dir.openNextFile(); entry; entry = dir.openNextFile()) {
    String name = entry.name();  // "<id>.565" (core 3.x returns the bare name)
    const int slash = name.lastIndexOf('/');
    if (slash >= 0) name = name.substring(slash + 1);
    entry.close();
    if (!name.endsWith(".565")) continue;
    const uint32_t id = (uint32_t)strtoul(name.c_str(), nullptr, 10);
    bool used = false;
    for (const Station &s : stations) used = used || s.id == id;
    if (!used) orphans.push_back(String(LOGO_DIR) + "/" + name);
  }
  dir.close();
  for (const String &path : orphans) fsys->remove(path);
}
