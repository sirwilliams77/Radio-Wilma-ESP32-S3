#include "WebPanel.h"

#include <DNSServer.h>
#include <Update.h>
#include <WebServer.h>
#include <WiFi.h>

#include "RadioApi.h"
#include "RadioUi.h"
#include "StationSearch.h"
#include "WebLogo.h"
#include "WebPage.h"
#include "Weather.h"

static WebServer server(80);
static DNSServer dnsServer;
static bool dnsActive = false;
static const char *PANEL_USER = "admin";

// Upload state (logo and firmware uploads arrive in chunks before the final handler).
static uint8_t *logoUpload = nullptr;
static size_t logoUploadSize = 0;
static bool logoUploadTooBig = false;
static bool uploadAuthorized = false;
static String updateError;

static bool authorized() {
  const String &password = radioPanelPassword();
  if (password.isEmpty()) return true;
  if (server.authenticate(PANEL_USER, password.c_str())) return true;
  server.requestAuthentication(BASIC_AUTH, "Radio", "Wymagane hasło panelu");
  return false;
}

static void sendJson(const String &json, int code = 200) {
  server.sendHeader("Cache-Control", "no-store");
  server.send(code, "application/json; charset=utf-8", json);
}

static void sendOk() { sendJson("{\"ok\":true}"); }

static void sendError(const String &message, int code = 400) {
  sendJson("{\"ok\":false,\"error\":\"" + jsonEscape(message) + "\"}", code);
}

static long argLong(const char *name, long fallback) {
  return server.hasArg(name) ? server.arg(name).toInt() : fallback;
}

// ---------------------------------------------------------------------------
// Handlers
// ---------------------------------------------------------------------------
static void handleRoot() {
  if (!authorized()) return;
  server.sendHeader("Cache-Control", "no-store, must-revalidate");
  server.send_P(200, "text/html; charset=utf-8", WEB_PAGE_HTML);
}

static void handleLogoImage() {  // no password: also shown on the login prompt page
  server.sendHeader("Cache-Control", "max-age=86400");
  server.send_P(200, "image/jpeg", (PGM_P)WEB_LOGO_JPG, WEB_LOGO_JPG_LEN);
}

// Live screen preview: http://radio.local/ekran (also for OBS: ?clean&ms=200)
// The radio sends only the band of rows that changed since the frame the browser has,
// optionally at half resolution (2x2 average) and/or in 8-bit colour (RGB332).
static const char SCREEN_PAGE_HTML[] PROGMEM = R"rawliteral(<!doctype html><html lang="pl"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><title>Ekran radia</title>
<style>
:root{--bg:#0b0b0d;--panel:#16161a;--line:#2a2a31;--text:#ece8e1;--dim:#9b958b;--amb:#ffb300}
*{box-sizing:border-box}html,body{margin:0;background:var(--bg);color:var(--text);font:14px system-ui,sans-serif}
header{display:flex;flex-wrap:wrap;gap:8px 14px;align-items:center;padding:12px 16px;border-bottom:1px solid var(--line)}
header b{font-size:15px;letter-spacing:1px;margin-right:auto}
label{display:flex;align-items:center;gap:6px;color:var(--dim);font-size:13px}
select,button{background:var(--panel);color:var(--text);border:1px solid var(--line);border-radius:8px;padding:6px 9px;font:inherit;cursor:pointer}
button:hover,select:hover{border-color:var(--amb)}
main{padding:16px;display:flex;justify-content:center}
#win{resize:both;overflow:hidden;width:min(720px,100%);aspect-ratio:3/2;min-width:160px;min-height:107px;max-width:100%;
 background:#000;border:1px solid var(--line);border-radius:12px;box-shadow:0 10px 40px rgba(0,0,0,.5);display:flex}
#win.fixed{resize:none;aspect-ratio:auto;overflow:auto;display:block}
canvas{display:block;width:100%;height:100%;object-fit:contain;margin:auto}
#win.fixed canvas{object-fit:fill}
#st{text-align:center;color:var(--dim);font-size:12px;padding:0 16px 16px}
body.clean header,body.clean #st{display:none}
body.clean main{padding:0;height:100vh}body.clean #win{width:100%;height:100%;max-width:none;aspect-ratio:auto;resize:none;border:0;border-radius:0;box-shadow:none}
</style></head><body>
<header><b>EKRAN RADIA</b>
<label>Obraz <select id="res"><option value="1">480&times;320</option><option value="2">240&times;160</option></select></label>
<label>Kolory <select id="bits"><option value="16">16-bit</option><option value="8">8-bit</option></select></label>
<label>Odświeżanie <select id="fps"><option value="1000">1 kl/s</option><option value="500">2 kl/s</option><option value="200">5 kl/s</option><option value="100">10 kl/s</option></select></label>
<label>Rozmiar <select id="zoom"><option value="fit">dopasuj do okna</option><option value="0.5">50%</option><option value="1">100%</option><option value="1.5">150%</option><option value="2">200%</option><option value="3">300%</option></select></label>
<label><input type="checkbox" id="px"> ostre piksele</label>
<button id="full">Pełny ekran</button></header>
<main><div id="win"><canvas id="c" width="480" height="320"></canvas></div></main><div id="st">Łączenie...</div>
<script>
const $=i=>document.getElementById(i),c=$('c'),x=c.getContext('2d'),win=$('win'),st=$('st'),q=new URLSearchParams(location.search);
let opt={res:'1',bits:'16',fps:'200',zoom:'fit',px:false};
try{Object.assign(opt,JSON.parse(localStorage.getItem('ekranOpt')||'{}'));}catch(e){}
if(q.has('ms'))opt.fps=String(Math.max(100,+q.get('ms')||200));
if(q.has('clean'))document.body.classList.add('clean');
if(q.has('pixel'))opt.px=true;
for(const k of['res','bits','fps','zoom'])if([...$(k).options].some(o=>o.value===opt[k]))$(k).value=opt[k];$('px').checked=opt.px;
let ver=-1,img=null,frames=0,bytes=0,ms=0,t0=performance.now();
function save(){try{localStorage.setItem('ekranOpt',JSON.stringify(opt));}catch(e){}}
function layout(){c.style.imageRendering=opt.px?'pixelated':'auto';
 if(opt.zoom==='fit'){win.classList.remove('fixed');win.style.width='';win.style.height='';c.style.width='';c.style.height='';}
 else{win.classList.add('fixed');const z=+opt.zoom;c.style.width=480*z+'px';c.style.height=320*z+'px';win.style.width=Math.min(480*z+2,innerWidth-32)+'px';win.style.height=(320*z+2)+'px';}}
for(const k of['res','bits','fps','zoom'])$(k).onchange=()=>{opt[k]=$(k).value;save();if(k==='res'||k==='bits')ver=-1;layout();};
$('px').onchange=()=>{opt.px=$('px').checked;save();layout();};
$('full').onclick=()=>{win.requestFullscreen?win.requestFullscreen():0;};
layout();
async function tick(){const t=performance.now();
 try{const r=await fetch('/api/screen?v='+ver+'&s='+opt.res+'&b='+opt.bits,{cache:'no-store'});
  if(r.status===200){const buf=await r.arrayBuffer(),w=+r.headers.get('X-W'),y=+r.headers.get('X-Y'),h=+r.headers.get('X-H'),b=+r.headers.get('X-B');
   if(c.width!==w){c.width=w;c.height=w*2/3;img=null;}
   if(!img||img.width!==w)img=x.createImageData(w,c.height);
   const d=img.data,n=w*h;let o=y*w*4;
   if(b===8){const p=new Uint8Array(buf);for(let i=0;i<n&&i<p.length;i++,o+=4){const v=p[i];d[o]=(v>>5)*255/7;d[o+1]=(v>>2&7)*255/7;d[o+2]=(v&3)*85;d[o+3]=255;}}
   else{const p=new Uint16Array(buf);for(let i=0;i<n&&i<p.length;i++,o+=4){const v=p[i];d[o]=(v>>11)*255/31;d[o+1]=(v>>5&63)*255/63;d[o+2]=(v&31)*255/31;d[o+3]=255;}}
   x.putImageData(img,0,0,0,y,w,h);ver=+r.headers.get('X-Frame');frames++;bytes+=buf.byteLength;ms+=performance.now()-t;}
  else if(r.status!==204)st.textContent='Podgląd niedostępny ('+r.status+')';
  const dt=t-t0;if(dt>2000){st.textContent=(frames*1000/dt).toFixed(1)+' kl/s · '+(frames?(bytes/frames/1024).toFixed(1):'0')+' KB/klatkę · '+(bytes/dt*1000/1024).toFixed(0)+' KB/s · pobieranie '+(frames?(ms/frames).toFixed(0):'–')+' ms';frames=0;bytes=0;ms=0;t0=t;}}
 catch(e){st.textContent='Brak połączenia z radiem';}
 setTimeout(tick,Math.max(0,+opt.fps-(performance.now()-t)));}
tick();</script></body></html>)rawliteral";

static void handleScreenPage() {
  if (!authorized()) return;
  server.sendHeader("Cache-Control", "no-store");
  server.send_P(200, "text/html; charset=utf-8", SCREEN_PAGE_HTML);
}

static const uint32_t PREVIEW_MIN_INTERVAL_MS = 90;  // at most ~10 frames/s, whatever clients ask
static uint32_t lastPreviewAt = 0;

static inline uint16_t averageRgb565(uint16_t a, uint16_t b, uint16_t c, uint16_t d) {
  const uint16_t r = ((a >> 11) + (b >> 11) + (c >> 11) + (d >> 11) + 2) >> 2;
  const uint16_t g = (((a >> 5) & 63) + ((b >> 5) & 63) + ((c >> 5) & 63) + ((d >> 5) & 63) + 2) >> 2;
  const uint16_t bl = ((a & 31) + (b & 31) + (c & 31) + (d & 31) + 2) >> 2;
  return (uint16_t)((r << 11) | (g << 5) | bl);
}

static void handleScreenData() {
  if (!authorized()) return;
  static const int16_t W = 480, H = 320;
  uint32_t version = 0;
  const uint16_t *screen = uiScreenCopy(version);
  if (screen == nullptr) return sendError("Brak pamięci PSRAM na podgląd", 503);
  const uint32_t *rowVersion = uiScreenRowVersions();
  const uint8_t scale = server.arg("s") == "2" ? 2 : 1;
  const bool eightBit = server.arg("b") == "8";
  const long clientVersion = server.hasArg("v") ? server.arg("v").toInt() : -1;
  server.sendHeader("Cache-Control", "no-store");
  server.sendHeader("Access-Control-Expose-Headers", "X-Frame, X-W, X-Y, X-H, X-B");
  const uint32_t now = millis();
  if ((clientVersion >= 0 && (uint32_t)clientVersion == version) || now - lastPreviewAt < PREVIEW_MIN_INTERVAL_MS) {
    server.send(204);  // nothing new (or asked too often): the browser keeps its picture
    return;
  }
  // Rows changed since the client's frame (whole screen for a new client or after a restart).
  int16_t y0 = 0, y1 = H;
  if (clientVersion >= 0 && (uint32_t)clientVersion < version) {
    y0 = H;
    y1 = 0;
    for (int16_t y = 0; y < H; ++y) {
      if (rowVersion[y] > (uint32_t)clientVersion) {
        if (y < y0) y0 = y;
        y1 = y + 1;
      }
    }
    if (y1 <= y0) {
      server.send(204);
      return;
    }
  }
  y0 = y0 / scale * scale;
  y1 = (y1 + scale - 1) / scale * scale;
  lastPreviewAt = now;
  const int16_t outW = W / scale, outY = y0 / scale, outH = (y1 - y0) / scale;
  const size_t total = (size_t)outW * outH * (eightBit ? 1 : 2);
  server.sendHeader("X-Frame", String(version));
  server.sendHeader("X-W", String(outW));
  server.sendHeader("X-Y", String(outY));
  server.sendHeader("X-H", String(outH));
  server.sendHeader("X-B", eightBit ? "8" : "16");
  server.setContentLength(total);
  server.send(200, "application/octet-stream", "");

  // Sent from loop() - the screen cannot change while this runs, so frames never tear.
  static uint8_t out[4096];
  size_t used = 0;
  for (int16_t y = y0; y < y1; y += scale) {
    const uint16_t *row = screen + (uint32_t)y * W;
    for (int16_t xx = 0; xx < W; xx += scale) {
      uint16_t v = row[xx];
      if (scale == 2) v = averageRgb565(row[xx], row[xx + 1], row[xx + W], row[xx + W + 1]);
      if (eightBit) {
        out[used++] = (uint8_t)(((v >> 13) << 5) | (((v >> 8) & 7) << 2) | ((v >> 3) & 3));
      } else {
        out[used++] = (uint8_t)(v & 0xFF);  // little-endian like the browser's Uint16Array
        out[used++] = (uint8_t)(v >> 8);
      }
      if (used + 2 > sizeof(out)) {
        server.sendContent((const char *)out, used);
        used = 0;
      }
    }
  }
  if (used) server.sendContent((const char *)out, used);
}

static void handleStatus() {
  if (!authorized()) return;
  sendJson(radioStatusJson());
}

static void handleGetStations() {
  if (!authorized()) return;
  sendJson(stationsToJson(radioStations(), true));
}

static void handlePostStations() {
  if (!authorized()) return;
  std::vector<Station> stations;
  String error;
  if (!stationsFromJson(server.arg("plain"), stations, error)) return sendError(error);
  if (!radioReplaceStations(stations, error)) return sendError(error, 500);
  sendJson(stationsToJson(radioStations(), true));
}

static void handlePlay() {
  if (!authorized()) return;
  const long index = argLong("i", -1);
  if (index < 0 || index >= (long)radioStations().size()) return sendError("Nieprawidłowy numer stacji");
  radioPlayIndex((int)index);
  sendOk();
}

static void handleStep() {
  if (!authorized()) return;
  radioStep(argLong("d", 1) < 0 ? -1 : 1);
  sendOk();
}

static void handlePause() {
  if (!authorized()) return;
  radioTogglePause();
  sendOk();
}

static void handleVolume() {
  if (!authorized()) return;
  radioSetVolume((int)argLong("v", 0));
  sendOk();
}

static void handleMute() {
  if (!authorized()) return;
  radioToggleMute();
  sendOk();
}

static Station *findStation(uint32_t id) {
  for (const Station &s : radioStations()) {
    if (s.id == id) return const_cast<Station *>(&s);
  }
  return nullptr;
}

static void handleGetLogo() {
  if (!authorized()) return;
  const uint32_t id = (uint32_t)argLong("id", 0);
  if (!logoExists(id)) return sendError("Brak logo", 404);
  File file = storeOpen(logoPath(id), "r");
  if (!file) return sendError("Błąd odczytu", 500);
  server.sendHeader("Cache-Control", "no-cache");
  server.streamFile(file, "application/octet-stream");
  file.close();
}

static void handleLogoUpload() {
  HTTPUpload &upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    uploadAuthorized = radioPanelPassword().isEmpty() ||
                       server.authenticate(PANEL_USER, radioPanelPassword().c_str());
    logoUploadSize = 0;
    logoUploadTooBig = false;
    if (logoUpload == nullptr) logoUpload = (uint8_t *)ps_malloc(LOGO_BYTES);
    if (logoUpload == nullptr) logoUpload = (uint8_t *)malloc(LOGO_BYTES);
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (!uploadAuthorized || logoUpload == nullptr) return;
    if (logoUploadSize + upload.currentSize > LOGO_BYTES) {
      logoUploadTooBig = true;
      return;
    }
    memcpy(logoUpload + logoUploadSize, upload.buf, upload.currentSize);
    logoUploadSize += upload.currentSize;
  }
}

static void handleLogoDone() {
  if (!authorized()) return;
  const uint32_t id = (uint32_t)argLong("id", 0);
  if (findStation(id) == nullptr) return sendError("Nie ma stacji o tym id");
  if (logoUpload == nullptr) return sendError("Brak pamięci", 500);
  if (logoUploadTooBig || logoUploadSize != LOGO_BYTES) {
    return sendError("Logo musi mieć dokładnie " + String(LOGO_BYTES) + " bajtów (RGB565 " +
                     String(LOGO_W) + "x" + String(LOGO_H) + ")");
  }
  if (!logoSave(id, logoUpload, logoUploadSize)) return sendError("Błąd zapisu logo", 500);
  radioLogoChanged(id);
  sendOk();
}

static void handleLogoDelete() {
  if (!authorized()) return;
  const uint32_t id = (uint32_t)argLong("id", 0);
  logoRemove(id);
  radioLogoChanged(id);
  sendOk();
}

static_assert(LOGO_BYTES >= WEATHER_ICON_BYTES, "upload buffer holds a logo or a weather icon");

static void handleGetWeatherIcon() {
  if (!authorized()) return;
  const long kind = argLong("kind", -1);
  if (kind < 0 || kind >= WEATHER_ICON_KINDS) return sendError("Zły rodzaj ikony");
  const uint16_t *icon = uiWeatherIcon((uint8_t)kind);
  server.sendHeader("Cache-Control", "no-cache");
  server.sendHeader("X-Own-Icon", uiWeatherIconIsOwn((uint8_t)kind) ? "1" : "0");
  server.send_P(200, "application/octet-stream", (PGM_P)icon, WEATHER_ICON_BYTES);
}

static void handleWeatherIconDone() {  // the data arrives through handleLogoUpload
  if (!authorized()) return;
  const long kind = argLong("kind", -1);
  if (kind < 0 || kind >= WEATHER_ICON_KINDS) return sendError("Zły rodzaj ikony");
  if (logoUpload == nullptr) return sendError("Brak pamięci", 500);
  if (logoUploadTooBig || logoUploadSize != WEATHER_ICON_BYTES) {
    return sendError("Ikona musi mieć dokładnie " + String(WEATHER_ICON_BYTES) + " bajtów (RGB565 " +
                     String(WEATHER_ICON_W) + "x" + String(WEATHER_ICON_H) + ")");
  }
  if (!weatherIconSave((uint8_t)kind, logoUpload, logoUploadSize)) return sendError("Błąd zapisu ikony", 500);
  radioWeatherIconChanged((uint8_t)kind);
  sendOk();
}

static void handleWeatherIconDelete() {
  if (!authorized()) return;
  const long kind = argLong("kind", -1);
  if (kind < 0 || kind >= WEATHER_ICON_KINDS) return sendError("Zły rodzaj ikony");
  weatherIconRemove((uint8_t)kind);
  radioWeatherIconChanged((uint8_t)kind);
  sendOk();
}

// Internet station search ------------------------------------------------------
static void handleSearchStatus() {
  if (!authorized()) return;
  sendJson(searchStatusJson());
}

static void handleSearchStart() {
  if (!authorized()) return;
  String error;
  if (!searchStart(server.arg("q"), error)) return sendError(error);
  sendOk();
}

static void handleSearchPlay() {
  if (!authorized()) return;
  const long index = argLong("i", -1);
  String error;
  if (index < 0 || index >= SEARCH_MAX_RESULTS) return sendError("Nieprawidłowy numer wyniku");
  if (!radioPlaySearchResult((uint8_t)index, error)) return sendError(error);
  sendOk();
}

static void handleSearchAdd() {
  if (!authorized()) return;
  const long index = argLong("i", -1);
  String error;
  if (index < 0 || index >= SEARCH_MAX_RESULTS) return sendError("Nieprawidłowy numer wyniku");
  if (!radioAddSearchResult((uint8_t)index, error)) return sendError(error);
  sendOk();
}

static void handleSettings() {
  if (!authorized()) return;
  sendJson(radioSettingsJson());
}

static void handleScan() {
  if (!authorized()) return;
  const int16_t result = WiFi.scanComplete();
  if (result == WIFI_SCAN_FAILED || (server.hasArg("start") && result != WIFI_SCAN_RUNNING)) {
    WiFi.scanDelete();
    WiFi.scanNetworks(true);
    return sendJson("{\"running\":true,\"networks\":[]}");
  }
  if (result == WIFI_SCAN_RUNNING) return sendJson("{\"running\":true,\"networks\":[]}");
  String json = "{\"running\":false,\"networks\":[";
  bool first = true;
  for (int16_t i = 0; i < result; ++i) {
    const String ssid = WiFi.SSID(i);
    if (ssid.isEmpty()) continue;
    if (!first) json += ',';
    first = false;
    json += "{\"ssid\":\"" + jsonEscape(ssid) + "\",\"rssi\":" + String(WiFi.RSSI(i)) +
            ",\"open\":" + (WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "true" : "false") + "}";
  }
  json += "]}";
  sendJson(json);
}

static void handleWifi() {
  if (!authorized()) return;
  const String ssid = server.arg("ssid");
  if (ssid.isEmpty() || ssid.length() > 32) return sendError("Podaj nazwę sieci (maks. 32 znaki)");
  const String password = server.arg("pass");
  if (!password.isEmpty() && (password.length() < 8 || password.length() > 63)) {
    return sendError("Hasło Wi-Fi musi mieć od 8 do 63 znaków (albo być puste)");
  }
  sendOk();
  delay(100);  // let the response leave before the radio reconnects
  radioSetWifi(ssid, password);
}

static void handlePassword() {
  if (!authorized()) return;
  const String password = server.arg("pass");
  if (!password.isEmpty() && password.length() < 4) return sendError("Hasło musi mieć co najmniej 4 znaki");
  radioSetPanelPassword(password);
  sendOk();
}

static void handleWeather() {
  if (!authorized()) return;
  const String place = server.arg("place");
  if (place.length() > 60) return sendError("Za długa nazwa miejscowości");
  weatherSetPlace(place);
  sendOk();
}

static void handleGetColors() {
  if (!authorized()) return;
  sendJson(radioColorsJson());
}

static void handlePostColors() {
  if (!authorized()) return;
  radioSetColors([](const char *key) { return server.hasArg(key) ? server.arg(key) : String(); },
                 server.hasArg("reset"));
  sendJson(radioColorsJson());
}

static void handleRestart() {
  if (!authorized()) return;
  sendOk();
  delay(300);
  ESP.restart();
}

static void handleUpdateUpload() {
  HTTPUpload &upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    uploadAuthorized = radioPanelPassword().isEmpty() ||
                       server.authenticate(PANEL_USER, radioPanelPassword().c_str());
    updateError = "";
    if (!uploadAuthorized) return;
    radioUpdateStarted();
    if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) updateError = Update.errorString();
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (!uploadAuthorized || !updateError.isEmpty()) return;
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
      updateError = Update.errorString();
    } else {
      radioUpdateProgress(upload.totalSize, server.header("Content-Length").toInt());
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (!uploadAuthorized || !updateError.isEmpty()) return;
    if (!Update.end(true)) updateError = Update.errorString();
  } else if (upload.status == UPLOAD_FILE_ABORTED) {
    Update.abort();
    updateError = "Przerwano wysyłanie";
  }
}

static void handleUpdateDone() {
  if (!authorized()) return;
  if (!updateError.isEmpty()) {
    radioUpdateFailed(updateError);
    return sendError("Aktualizacja nieudana: " + updateError, 500);
  }
  sendOk();
  delay(500);
  ESP.restart();
}

static void handleNotFound() {
  // In access point mode phones open random URLs (captive portal checks): send them the panel.
  if (WiFi.getMode() & WIFI_AP) {
    server.sendHeader("Location", "http://" + WiFi.softAPIP().toString() + "/", true);
    server.send(302, "text/plain", "");
    return;
  }
  if (!authorized()) return;
  sendError("Nie znaleziono", 404);
}

// ---------------------------------------------------------------------------
void webBegin() {
  const char *headers[] = {"Content-Length"};
  server.collectHeaders(headers, 1);

  server.on("/", HTTP_GET, handleRoot);
  server.on("/logo.jpg", HTTP_GET, handleLogoImage);
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/ekran", HTTP_GET, handleScreenPage);
  server.on("/api/screen", HTTP_GET, handleScreenData);
  server.on("/api/stations", HTTP_GET, handleGetStations);
  server.on("/api/stations", HTTP_POST, handlePostStations);
  server.on("/api/play", HTTP_POST, handlePlay);
  server.on("/api/step", HTTP_POST, handleStep);
  server.on("/api/pause", HTTP_POST, handlePause);
  server.on("/api/volume", HTTP_POST, handleVolume);
  server.on("/api/mute", HTTP_POST, handleMute);
  server.on("/api/logo", HTTP_GET, handleGetLogo);
  server.on("/api/logo", HTTP_POST, handleLogoDone, handleLogoUpload);
  server.on("/api/logo/delete", HTTP_POST, handleLogoDelete);
  server.on("/api/search", HTTP_GET, handleSearchStatus);
  server.on("/api/search", HTTP_POST, handleSearchStart);
  server.on("/api/search/play", HTTP_POST, handleSearchPlay);
  server.on("/api/search/add", HTTP_POST, handleSearchAdd);
  server.on("/api/wxicon", HTTP_GET, handleGetWeatherIcon);
  server.on("/api/wxicon", HTTP_POST, handleWeatherIconDone, handleLogoUpload);
  server.on("/api/wxicon/delete", HTTP_POST, handleWeatherIconDelete);
  server.on("/api/settings", HTTP_GET, handleSettings);
  server.on("/api/scan", HTTP_GET, handleScan);
  server.on("/api/wifi", HTTP_POST, handleWifi);
  server.on("/api/password", HTTP_POST, handlePassword);
  server.on("/api/weather", HTTP_POST, handleWeather);
  server.on("/api/colors", HTTP_GET, handleGetColors);
  server.on("/api/colors", HTTP_POST, handlePostColors);
  server.on("/api/restart", HTTP_POST, handleRestart);
  server.on("/api/update", HTTP_POST, handleUpdateDone, handleUpdateUpload);
  server.onNotFound(handleNotFound);
  server.begin();
}

void webCaptivePortal(bool enable) {
  if (enable && !dnsActive) {
    dnsServer.start(53, "*", WiFi.softAPIP());  // every name resolves to the radio
    dnsActive = true;
  } else if (!enable && dnsActive) {
    dnsServer.stop();
    dnsActive = false;
  }
}

void webLoop() {
  if (dnsActive) dnsServer.processNextRequest();
  server.handleClient();
}
