#pragma once
// Verification of HTTPS servers, shared by Weather.cpp (Open-Meteo) and StationSearch.cpp
// (radio-browser.info).
//
// Chain of trust: the Mozilla root CA bundle built into ESP32 Arduino Core 3.x (ESP-IDF
// esp_crt_bundle, compiled into the core's libraries and stored in flash). Both services use
// certificates from public CAs (Let's Encrypt, Google Trust Services) that are renewed every
// few weeks, so no server certificate is pinned and nothing has to be replaced by hand: the
// bundle follows the core version. mbedTLS also checks the host name (subjectAltName).
//
// Validity dates: the prebuilt core libraries are compiled without MBEDTLS_HAVE_TIME_DATE, so
// mbedTLS does not reject expired or not-yet-valid certificates on its own.
// tlsCertificateDatesOk() checks the dates of the server certificate against the NTP clock.
#include <Arduino.h>
#include <NetworkClientSecure.h>
#include <time.h>

// Same threshold as the rest of the sketch: earlier means the clock has not been set by NTP.
static const time_t TLS_CLOCK_VALID_AFTER = 1700000000;

// Call on every new client before connecting: verify the server against the built-in bundle.
static inline void tlsUseTrustedRoots(NetworkClientSecure &client) {
  client.useBuiltinCACertBundle();
}

static inline bool tlsClockValid() { return time(nullptr) >= TLS_CLOCK_VALID_AFTER; }

// Seconds since 1970-01-01 UTC of a certificate date (proleptic Gregorian calendar).
static inline int64_t tlsEpochSeconds(const mbedtls_x509_time &t) {
  int y = t.year;
  const int m = t.mon;
  y -= m <= 2 ? 1 : 0;
  const int era = (y >= 0 ? y : y - 399) / 400;
  const int yoe = y - era * 400;
  const int doy = (153 * (m > 2 ? m - 3 : m + 9) + 2) / 5 + t.day - 1;
  const int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  const int64_t days = (int64_t)era * 146097 + doe - 719468;
  return days * 86400 + t.hour * 3600 + t.min * 60 + t.sec;
}

// Call after a request succeeded and before its answer is used (the connection must still be
// open). true: the server certificate is valid now, or the clock is not set yet (then only the
// chain and the host name are checked - logged). false: expired / not yet valid / missing.
static inline bool tlsCertificateDatesOk(NetworkClientSecure &client, const char *service,
                                         String &error) {
  const mbedtls_x509_crt *cert = client.getPeerCertificate();
  if (cert == nullptr) {
    error = "brak certyfikatu serwera";
    Serial.printf("%s: TLS - no server certificate\n", service);
    return false;
  }
  const time_t now = time(nullptr);
  if (now < TLS_CLOCK_VALID_AFTER) {
    Serial.printf("%s: TLS - clock not set yet, certificate dates not checked\n", service);
    return true;
  }
  const int64_t from = tlsEpochSeconds(cert->valid_from);
  const int64_t to = tlsEpochSeconds(cert->valid_to);
  if ((int64_t)now < from || (int64_t)now > to) {
    error = (int64_t)now > to ? "certyfikat serwera wygasł" : "certyfikat serwera jeszcze nieważny";
    Serial.printf("%s: TLS - certificate valid %04d-%02d-%02d .. %04d-%02d-%02d, rejected\n", service,
                  cert->valid_from.year, cert->valid_from.mon, cert->valid_from.day,
                  cert->valid_to.year, cert->valid_to.mon, cert->valid_to.day);
    return false;
  }
  return true;
}

// Serial log of the reason of a failed TLS connection (untrusted certificate, wrong host name,
// handshake error); the messages for the user stay as they were.
static inline void tlsLogError(NetworkClientSecure &client, const char *service) {
  char text[100];
  const int code = client.lastError(text, sizeof(text));
  if (code != 0) Serial.printf("%s: TLS error -0x%04X: %s\n", service, (unsigned)-code, text);
}
