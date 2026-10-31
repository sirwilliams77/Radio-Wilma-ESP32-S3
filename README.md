# Radio Wilma – radio internetowe na ESP32-S3

Radio internetowe na ESP32-S3 WROOM N16R8 z wyświetlaczem ST7796 (480×320, SPI), przetwornikiem
PCM5102A (I2S) i dwoma enkoderami. Sterowanie także z przeglądarki: **http://radio.local**.

## Co jest w tym katalogu

| Plik / katalog | Opis |
| --- | --- |
| `Radio_Wilma.ino` | Logika radia: Wi-Fi, punkt konfiguracji, NTP, audio, enkodery, metadane |
| `RadioUi.h/.cpp` | Ekran: zegar, pogoda, stacja, utwór, equalizer, paski |
| `WebPanel.h/.cpp`, `WebPage.h` | Panel WWW (serwer, API, strona) |
| `StationStore.h/.cpp` | Lista stacji i logo w pamięci flash (JSON + RGB565) |
| `Weather.h/.cpp` | Pogoda z Open-Meteo (osobne zadanie, co 15 min) |
| `StationSearch.h/.cpp` | Wyszukiwanie stacji w radio-browser.info (osobne zadanie) |
| `RadioApi.h` | Funkcje radia używane przez panel |
| `UiTypes.h` | Struktury czcionek/ikon, rozmiary logo |
| `UiFonts.h`, `UiIcons.h`, `UiImages.h`, `UiWeather.h`, `WebLogo.h`, `StationLogos.h` | **Wygenerowane** grafiki (nie edytować ręcznie) |
| `tools/make_ui_assets.py` | Generator powyższych plików |
| `tools/fonts/` | Czcionki Roboto (Regular, Medium, Bold, Black) + licencja OFL |
| `tools/weather_icons/` | Ikony pogody (PNG) – **do podmiany** |
| `assets/radio_wilma_logo.jpg` | Logo panelu WWW |
| `secrets.example.h` | Wzór opcjonalnego `secrets.h` |

Do samej kompilacji wystarczą pliki z głównego katalogu. `tools/` i `assets/` są potrzebne tylko
do ponownego wygenerowania grafik.

## Kompilacja (Arduino IDE)

0. Pobierz projekt i upewnij się, że katalog szkicu nazywa się dokładnie **`Radio_Wilma`**
   (Arduino wymaga, by nazwa katalogu była identyczna z nazwą pliku `Radio_Wilma.ino`, łącznie
   z wielkością liter):
   ```
   git clone https://github.com/sirwilliams77/Radio-Wilma-ESP32-S3.git
   ```
   Przy pobieraniu jako ZIP (*Code → Download ZIP*) GitHub tworzy katalog `Radio-Wilma-ESP32-S3-main` –
   zmień jego nazwę na `Radio_Wilma`, aby Arduino IDE poprawnie rozpoznało szkic.
1. Płytka: **esp32** by Espressif, wersja 3.x → **ESP32S3 Dev Module**.
   Flash Size **16MB**, Flash Mode **QIO 80MHz**, PSRAM **OPI PSRAM**,
   Partition Scheme **16M Flash (3MB APP/9.9MB FATFS)** (partycja na stacje/logo + dwie partycje
   programu potrzebne do aktualizacji przez Wi-Fi).
2. Biblioteki: **GFX Library for Arduino** (testowane 1.6.9) i **ESP32-audioI2S 4.0.0**
   (schreibfaul1). Pozostałe (WebServer, DNSServer, ESPmDNS, FFat, LittleFS, Preferences, Update,
   HTTPClient) są w pakiecie płytki ESP32.
3. Opcjonalnie skopiuj `secrets.example.h` jako `secrets.h` i wpisz sieć Wi-Fi na pierwszy start.
   Bez tego pliku radio uruchomi punkt dostępowy **Radio-Wilma** (domyślne hasło `radio1234`). To hasło jest publiczne i służy wyłącznie do konfiguracji pierwszego uruchomienia; po skonfigurowaniu Wi-Fi wyłącz punkt dostępowy lub zmień hasło w kodzie przed użyciem w miejscu publicznym. Ekran radia pokaże wtedy instrukcję:
   połącz się z tą siecią, otwórz `http://192.168.4.1` i w Ustawieniach > Wi-Fi wybierz swoją sieć.
4. Wgraj szkic. Monitor portu: 115200 bodów.

## Ekran

- Zegar Roboto Bold (biały, z białym dwukropkiem), po lewej temperatura i miejscowość,
  po prawej duża ikona pogody.
- Nazwa stacji, opis, wykonawca i tytuł wyśrodkowane; za długie teksty przewijają się płynnie.
- Equalizer: 15 pasm z biblioteki audio, segmenty zielone/żółte/czerwone ze znacznikami szczytów.
- Dolny pasek: adres IP radia, numer stacji, bitrate, gatunek, strefa czasowa.
- Każdy blok ekranu (pogoda, zegar, logo, stacja, płyta, utwór, equalizer) w ramce z
  zaokrąglonymi rogami (kolor: Ustawienia > Wygląd > Ramki pól).
- Obok utworu kręci się płyta winylowa, gdy gra muzyka; przy pauzie i łączeniu stoi.

## Panel WWW

- **Odtwarzacz**: logo i nazwa stacji, utwór, przyciski, głośność oraz lista stacji
  z wyszukiwarką. Kliknięcie stacji ją włącza; przeciągnięcie obrazka na stację ustawia jej logo.
- **Logo stacji** (pole 88×66, tak jak na ekranie radia), w edytorze stacji do wyboru:
  - dopasowanie: **Dopasuj** (domyślnie – całe logo, mniejsze niż pole zostaje w oryginalnym
    rozmiarze), **Wypełnij** (przycina), **Rozciągnij**;
  - tło wolnego miejsca: **Auto** (kolor z krawędzi loga), **Czarne**, **Ciemnoszare**.
  - Ustawienia → Stacje: pole „Upuść tutaj logo kilku stacji naraz” – pliki są przypisywane do
    stacji po nazwie pliku (np. `rmf_fm.png` → RMF FM), przypisanie można poprawić przed zapisem.
- **Ustawienia** (kafelki): Wi-Fi (szukanie sieci), Stacje (dodawanie, edycja, logo, kolejność,
  eksport/import), Pogoda (miejscowość), Hasło panelu (użytkownik `admin`), Aktualizacja (plik
  `.bin`: Szkic → Eksportuj skompilowany plik binarny), System (pamięć, restart).

## Zmiana grafik

Wymagany Python 3 z Pillow (`pip install pillow`). W katalogu szkicu:

```
python tools/make_ui_assets.py
```

- **Ikony pogody z panelu**: Ustawienia > Pogoda > Ikony pogody. Każdą z 10 ikon można zastąpić
  dowolnym obrazem (najlepiej PNG z przezroczystym tłem; przeglądarka dopasowuje go do 78×60 na
  czarnym tle). Pliki trafiają na partycję FAT (`/wx/<rodzaj>.565`), „Przywróć” wraca do wbudowanej.
- **Ikony pogody**: podmień dowolny plik w `tools/weather_icons/` (`clear_day.png`,
  `clear_night.png`, `partly_day.png`, `partly_night.png`, `cloudy.png`, `fog.png`,
  `drizzle.png`, `rain.png`, `snow.png`, `storm.png`). Dowolny rozmiar i kolory, przezroczyste tło;
  obraz jest dopasowywany do 78×60 na czarnym tle. Usunięty plik zostanie odtworzony w domyślnym
  wyglądzie. Potem uruchom skrypt i skompiluj szkic.
- **Logo panelu**: podmień `assets/radio_wilma_logo.jpg` (najlepiej mały JPG, do ~30 KB).

## Podgląd ekranu: http://radio.local/ekran

Okno z podglądem (rozmiar można zmieniać, powiększenie 50–300 %, pełny ekran). Do wyboru: obraz
480×320 lub 240×160, kolory 16- lub 8-bit, odświeżanie 1/2/5/10 kl/s; ustawienia zapamiętuje
przeglądarka. Radio wysyła tylko pas wierszy, który zmienił się od poprzedniej klatki (zwykle
equalizer i płyta: ok. 70 z 320 wierszy) i najwyżej ~10 klatek/s. Pod oknem widać pomiar: kl/s,
KB na klatkę, KB/s i czas pobrania. Dla OBS: `/ekran?clean` (sam obraz), `?ms=200` (odstęp klatek).

## Wyszukiwanie stacji (panel WWW)

Ustawienia > **Szukaj stacji**: wpisz nazwę (np. „jazz”, „BBC”). Radio pyta społecznościową bazę
[radio-browser.info](https://www.radio-browser.info/) (bezpłatna, bez klucza) w osobnym zadaniu, więc
odtwarzanie się nie zatrzymuje. Wyniki (do 25, od najpopularniejszych) pokazują nazwę, kraj, język,
gatunek, format i bitrate.

- **Graj** – stacja gra tymczasowo, lista ulubionych się nie zmienia (w dolnym pasku numer 0/N).
  Enkoder stacji wraca do ulubionych, po restarcie gra ostatnia ulubiona.
- **Do ulubionych** – dopisuje stację na koniec listy (limit 100, bez duplikatów adresu).
- Przyciski są tylko przy formatach obsługiwanych przez bibliotekę audio (MP3, AAC/AAC+, OGG,
  Vorbis, Opus, FLAC). HLS (`.m3u8`) i inne formaty są oznaczone jako nieobsługiwane.
- Komunikaty: „Brak wyników”, „Brak połączenia z internetem”, „Serwis wyszukiwania niedostępny”, a na
  ekranie radia „Strumień niedostępny”, „Brak dźwięku ze strumienia” (po 15 s bez dźwięku) i
  „Połączenie ze strumieniem przerwane” (ponowna próba jak dotąd, co 30 s).
- Pamięć: wyniki ~13 KB i odpowiedź serwera do 96 KB w PSRAM tylko na czas wyszukiwania; zadanie
  wyszukiwania kończy się samo i zwalnia swój stos.

## Pogoda

Dane: [Open-Meteo](https://open-meteo.com/) – bezpłatne do użytku niekomercyjnego, bez klucza.
Miejscowość ustawiasz w panelu; radio raz wyszukuje jej współrzędne, potem co 15 minut pobiera
aktualną pogodę.

## Piny

| Sygnał | GPIO |
| --- | ---: |
| TFT SCK / MOSI / DC / CS / RST | 12 / 11 / 9 / 10 / 14 |
| Podświetlenie | 21 |
| PCM5102A BCLK / LRC / DIN | 17 / 15 / 16 |
| Enkoder 1 S1 / S2 / KEY | 5 / 4 / 6 |
| Enkoder 2 S1 / S2 / KEY | 18 / 7 / 3 |

Enkoder 1: tylko głośność (obrót). Enkoder 2: tylko zmiana stacji (obrót). Przyciski enkoderów
(KEY) nie mają funkcji; wyciszenie i pauza są dostępne w panelu WWW.

Obrót enkodera głośności otwiera ekran głośności (duża cyfra i skala), a obrót enkodera
stacji — listę stacji z podświetloną pozycją. Stacja zaczyna
grać, gdy pokrętło zatrzyma się na 0,25 s. Po 2 s bez ruchu wraca ekran główny.

## Licencja

Własny kod źródłowy Radio Wilma jest udostępniany na warunkach **GNU General Public License v3.0 (GPL-3.0)**. Pełny tekst licencji znajduje się w pliku `LICENSE`.

Licencje elementów zewnętrznych pozostają odrębne:
- Czcionki Roboto oraz wygenerowane z nich bitmapy: SIL Open Font License 1.1 (`tools/fonts/OFL.txt`).
- Ikony pogody Meteocons: MIT, © Bas Milius (`tools/weather_icons/LICENSE-meteocons.txt`).
- Biblioteka ESP32-audioI2S: GPL-3.0; należy zainstalować ją osobno zgodnie z instrukcją powyżej.
- Biblioteka Arduino_GFX: licencja BSD; należy zainstalować ją osobno zgodnie z instrukcją powyżej.

Nie dołączaj do repozytorium własnego pliku `secrets.h` ani prywatnych danych dostępowych. Przed dalszym rozpowszechnianiem zmodyfikowanych wersji zachowaj wymagane informacje o prawach autorskich i licencje komponentów zewnętrznych.

---
© 2026 Sir Williams · Radio Wilma
