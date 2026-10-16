/*
 * K.O.R.N. Rev3.1 – Modularer Sketch (DS3231)
 * Dateien:
 *  - main.ino      (Setup/Loop, Orchestrierung, Timer/Status)
 *  - motor.ino     (Motorsteuerung/Relais/ENA)
 *  - ap.ino        (Access Point / Netzwerk)
 *  - homepage.ino  (Webserver & Routen)
 *
 * Hardware (README):
 *  - DS3231: SDA=A4, SCL=A5, VCC=5V, GND
 *  - Motor:  PUL=D2, DIR=D3, ENA=D5, Relais=D8
 *  - Buzzer: D6 aktiv (HIGH=Ton)
 *  - Manuell: D10 (INPUT_PULLUP), D11 (OUTPUT LOW) -> Kurzschluss löst aus
 *
 * Benötigte Bibliotheken (Arduino IDE Library Manager):
 *  - "RTClib by Adafruit" Version 2.1.1+ (für DS3231 RTC)
 *  - "AccelStepper" Version 1.61+       (für Schrittmotor-Steuerung)
 *  - WiFiS3 (vorinstalliert)             (für Arduino UNO R4 WiFi)
 *  - EEPROM (vorinstalliert)             (für Backup-Speicherung)
 *  - avr/wdt.h (nur AVR, vorinstalliert) (für Watchdog, nicht auf UNO R4)
 *
 * Board: Arduino UNO R4 WiFi
 * Board Package: Arduino UNO R4 Boards (arduino:renesas_uno)
 */

// Vorwärtsdeklaration: nötig, damit Arduino-Präprozessor-Prototypen KConfig kennen
struct KConfig;

#include <Arduino.h>
#include <EEPROM.h>     // EEPROM-Backup für Konfiguration
// Watchdog nur für AVR-basierte Arduinos (UNO R4 nutzt anderen Mechanismus)
#ifdef __AVR__
  #include <avr/wdt.h>
  #define WATCHDOG_ENABLED
#endif
#include <AccelStepper.h>
#include <WiFiS3.h>
#include <Wire.h>            // I2C-Bus für DS3231
#include <RTClib.h>          // Adafruit RTClib für DS3231

// ============================================================================
// Defaults & Flags (aus README)
// ============================================================================
const int FUETTERUNG_STUNDE_1 = 7;
const int FUETTERUNG_MINUTE_1 = 1;
const int FUETTERUNG_STUNDE_2 = 16;
const int FUETTERUNG_MINUTE_2 = 1;
const bool ZWEITE_ZEIT_AKTIV = true;    // zweite Zeit aktiv
const int FEED_STEPS_PER_SEC = 1000;    // feste Schrittfrequenz für zeitbasierte Fütterung
const bool MOTOR_DIR_CW = false;        // false = CCW (links), true = CW (rechts)

// Serielle Minimal-Ausgabe Intervall (Sekunden)
const uint16_t STATUS_INTERVAL_S = 60;
// Mindestabstand zwischen zwei Fütterungen (Minuten) – schützt vor Doppel-Triggern
const uint16_t MIN_GAP_MIN = 2;
// Arming-Window nach Boot: geplante Fütterungen erst nach diesem Zeitfenster erlauben
const uint32_t ARMING_WINDOW_MS = 60000UL;
// WLAN-Reconnect Intervall (Millisekunden) - 0 = deaktiviert für Off-Grid AP-Modus
const uint32_t WIFI_CHECK_INTERVAL_MS = 0UL;

// Quelle der letzten/kommenden Fütterung (für Status/Anzeige)
const uint8_t SRC_NONE = 0;
const uint8_t SRC_MANUAL = 1;
const uint8_t SRC_WEB = 2;
const uint8_t SRC_SCHED1 = 3;
const uint8_t SRC_SCHED2 = 4;

// DS3231 über I2C (Arduino UNO R4: SDA=A4, SCL=A5)
static RTC_DS3231 rtc;

// Manuelle Auslösung (Kurzschluss 10↔11)
#define MANUAL_TRIGGER_PIN 10  // INPUT_PULLUP
#define MANUAL_GROUND_PIN 11   // OUTPUT LOW

// Vorwärtsdeklarationen der Modul-APIs
void motorInit();
void motorEnable(bool on);
void motorReset();
bool motorFeed(int steps, bool dirCW);

void apInit(const char* ssid, const char* pass);
void apCheckAndReconnect();
void webInit();
void webHandleClient();
// RTC-Sync auf Kompilierzeit
void rtcSyncToCompile();

// Kompakter Status-Snapshot für die Web-UI (RAM-schonend, keine Strings)
void getStatusSnapshot(bool &rtcOk, int &nowH, int &nowM, int &nowS,
                       int &lastH, int &lastM,
                       int &n1H, int &n1M, int &n2H, int &n2M,
                       int &c1H, int &c1M, int &c2H, int &c2M,
                       bool &a2, int &steps, uint8_t &lastSrc, bool &delayWarning,
                       int &actualH, int &actualM, int &actualS);

// Hilfsfunktion zur Berechnung der nächsten Fütterungszeiten
static void computeNextTimes(int nowH, int nowM, int &n1H, int &n1M, int &n2H, int &n2M);

// Sofortfütterung zentral aus main anstoßen (für Web/Hardware)
bool requestImmediateFeed();

// Konfig-Getter/Setter für Web-UI (entkoppelt von struct in anderen Modulen)
int cfgGetH1();
int cfgGetM1();
int cfgGetH2();
int cfgGetM2();
bool cfgGetActive2();
int cfgGetSteps();
void cfgUpdateAndSave(uint8_t h1, uint8_t m1, uint8_t h2, uint8_t m2, bool active2, uint32_t steps);
// Batterie-Indikator (softwarebasiert): RTC-Zeit gültig
bool batteryLikelyOK();

// ============================================================================
// Serielle Minimal-Ausgabe
// ============================================================================
static unsigned long lastStatusMs = 0;
static unsigned long lastWifiCheckMs = 0;
static int lastFeedHour = -1;     // letzte Fütterung (HH)
static int lastFeedMinute = -1;   // letzte Fütterung (MM)
static int lastFeedSecond = -1;   // letzte Fütterung (SS)
static int lastFeedSteps = -1;    // letzte Fütterung (Schritte)
static uint8_t lastKnownDay = 0;  // zur Erkennung des Tageswechsels
static bool fedToday1 = false;    // Marker: Fütterung 1 heute ausgeführt
static bool fedToday2 = false;    // Marker: Fütterung 2 heute ausgeführt
static bool feedingInProgress = false; // einfache Reentrancy-Sperre
static volatile uint8_t nextFeedSrc = SRC_NONE; // vor requestImmediateFeed setzen
static uint8_t lastFeedSrc = SRC_NONE;          // für Statusanzeige
// Manuell-Trigger-Entprellung/Hold und Lockout
static unsigned long manualLowSinceMs = 0;        // Zeitpunkt seit dem D10=LOW ist
static unsigned long manualLockoutUntilMs = 0;    // bis wann manuelle Trigger ignoriert werden
static bool manualConsumed = false;               // einmalige Auslösung pro Tastendruck (bis Release)
// Pending-Flags für geplante Fütterungen, falls sie während einer laufenden Fütterung fällig werden
static bool pendingSched1 = false;
static bool pendingSched2 = false;
// Bootzeit (kann optional genutzt werden, z.B. für Arming-Delays)
static unsigned long bootMs = 0;
// Web: asynchrones Feed-Request-Flag (verhindert blockierende HTTP-Antworten)
static volatile bool webFeedRequested = false;
// Scheduler-Zustand für sekundengenaue Auslösung
static int lastNowSec = -1;           // Sekunden des Tages der letzten Prüfung (0..86399)
static unsigned long lastNowMs = 0;   // millis()-Referenz für letzte Prüfung
// firstSchedCheck entfernt – Catch-up nur via pending

// =========================================================================
// Konfiguration (Zeiten, Aktiv-Flag, Steps) – Ziel: EEPROM (Persistenz)
// =========================================================================
// Web-Helfer: von homepage.ino aufrufen, um asynchrones Feed zu markieren
void requestFeedFromWebAsync() {
  if (!feedingInProgress) {
    webFeedRequested = true;
  }
}
struct KConfig {
  uint16_t magic;       // 0xBEEF als Marker
  uint8_t v;            // Versionsbyte
  uint8_t h1, m1;       // Zeit 1
  uint8_t h2, m2;       // Zeit 2
  uint8_t active2;      // 0/1 zweite Zeit aktiv
  uint32_t steps;       // Motor-Schritte (erweitert für bis zu 600s Laufzeit)
  uint16_t crc;         // einfache Prüfsumme
};

static KConfig gCfg;         // aktuelle Konfiguration im RAM
static bool gCfgFromEeprom = false; // Herkunft: true=aus EEPROM geladen, false=nicht aus EEPROM

// EEPROM-Speicheradresse für Konfiguration
#define EEPROM_CFG_ADDR 0

// Merker: DS3231 wurde beim Start erfolgreich initialisiert (I2C erreichbar)
static bool gRtcPresent = false;
static bool gRtcPowerLossDetected = false;

// Prüft, ob die DS3231 eine plausibel gültige Zeit liefert
static bool rtcIsValid() {
  if (!gRtcPresent) return false;
  DateTime now = rtc.now();
  int y = now.year();
  return (y >= 2024 && y <= 2099);
}

static void rtcApplyTime(const DateTime &value) {
  if (!gRtcPresent) return;

  bool sameDate = false;
  if (rtcIsValid()) {
    DateTime current = rtc.now();
    sameDate = current.year() == value.year()
            && current.month() == value.month()
            && current.day() == value.day();
  }

  rtc.adjust(value);
  lastNowSec = value.hour() * 3600 + value.minute() * 60 + value.second();
  lastNowMs = millis();
  lastKnownDay = value.day();
  pendingSched1 = false;
  pendingSched2 = false;
  if (!sameDate) {
    fedToday1 = false;
    fedToday2 = false;
  }
}

// Liefert aktuelle Sekunden des Tages (0..86399) und optional DateTime.
// Rückgabe true: RTC-Zeit gültig; false: millis()-Fallback (oder keine Zeit -> outSec = -1).
static bool getNow(DateTime &outDt, int &outSec) {
  // Retry, falls RTC beim Start nicht erkannt wurde (periodisch alle 5s)
  static unsigned long lastRtcInitMs = 0;
  if (!gRtcPresent && (lastRtcInitMs == 0 || (millis() - lastRtcInitMs) >= 5000UL)) {
    lastRtcInitMs = millis();
    gRtcPresent = rtc.begin();
    if (gRtcPresent) {
      // RTC neu erkannt, aber evtl. ohne gültige Zeit -> Kompilierzeit setzen
      outDt = rtc.now();
      int y = outDt.year();
      if (y < 2024 || y > 2099) {
        rtcApplyTime(DateTime(__DATE__, __TIME__));
      }
      outDt = rtc.now();
      y = outDt.year();
      if (y >= 2024 && y <= 2099) {
        outSec = (int)outDt.hour() * 3600 + (int)outDt.minute() * 60 + (int)outDt.second();
        return true;
      }
    }
  }

  if (gRtcPresent) {
    outDt = rtc.now();
    int y = outDt.year();
    if (y >= 2024 && y <= 2099) {
      outSec = (int)outDt.hour() * 3600 + (int)outDt.minute() * 60 + (int)outDt.second();
      return true;
    }
  }

  // Fallback auf millis(), falls wir jemals eine gültige Zeit hatten
  if (lastNowSec >= 0 && lastNowMs > 0) {
    unsigned long elapsed = (millis() - lastNowMs) / 1000UL;
    outSec = (int)((lastNowSec + elapsed) % 86400UL);
    return false;
  }

  outSec = -1;
  return false;
}

static uint8_t daysInMonth(uint16_t year, uint8_t month) {
  static const uint8_t days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (month == 2 && ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0)) return 29;
  return days[month - 1];
}

bool batteryLikelyOK() {
  // Heuristik: RTC erreichbar, Zeit gültig und beim Start kein Stromverlust erkannt
  return rtcIsValid() && !gRtcPowerLossDetected;
}

static uint16_t cfgChecksum(const KConfig &c) {
  // Sehr einfache Prüfsumme über die payload-Felder (ohne magic, v, crc)
  uint32_t s = 0;
  s += c.h1 + c.m1 + c.h2 + c.m2 + c.active2;
  s += c.steps & 0xFF; s += (c.steps >> 8) & 0xFF; s += (c.steps >> 16) & 0xFF; s += (c.steps >> 24) & 0xFF;
  return (uint16_t)((s & 0xFFFFu) ^ 0xA5A5u);
}

// Auf Anforderung: RTC auf Kompilierzeit setzen (manueller Sync)
void rtcSyncToCompile() {
  rtcApplyTime(DateTime(__DATE__, __TIME__));
}

// Auf Anforderung: RTC auf vom Client übergebene lokale Gerätezeit setzen
void rtcSet(uint16_t y, uint8_t m, uint8_t d, uint8_t H, uint8_t M, uint8_t S) {
  if (!gRtcPresent || y < 2024 || y > 2099) return;
  if (m < 1 || m > 12) return;
  if (d < 1 || d > daysInMonth(y, m)) return;
  if (H > 23 || M > 59 || S > 59) return;
  rtcApplyTime(DateTime(y, m, d, H, M, S));
}

static void cfgApplyDefaults() {
  gCfg.magic = 0xBEEF;
  gCfg.v = 1;
  gCfg.h1 = FUETTERUNG_STUNDE_1;
  gCfg.m1 = FUETTERUNG_MINUTE_1;
  gCfg.h2 = FUETTERUNG_STUNDE_2;
  gCfg.m2 = FUETTERUNG_MINUTE_2;
  gCfg.active2 = ZWEITE_ZEIT_AKTIV ? 1 : 0;
  // Standard 5 Sekunden Laufzeit -> Schritte = 5s * FEED_STEPS_PER_SEC
  gCfg.steps = 5 * FEED_STEPS_PER_SEC;
  gCfg.crc = cfgChecksum(gCfg);
}

static void cfgApplyToRuntime() {
  // Setzt die Laufzeit-Parameter aus gCfg
  // Hinweis: Konstanten FUETTERUNG_* sind Defaults; Laufzeit nutzt gCfg.
  // Für einfache Integration verwenden wir gCfg direkt in Berechnungen.
}

// Konfiguration aus EEPROM laden (DS3231 hat kein nutzbares RAM)
// Bei ungültigen/fehlenden Daten werden Defaults geladen
static void cfgLoadFromEeprom() {
  KConfig backup;
  EEPROM.get(EEPROM_CFG_ADDR, backup);
  bool valid = backup.magic == 0xBEEF
            && backup.v == 1
            && backup.h1 <= 23 && backup.m1 <= 59
            && backup.h2 <= 23 && backup.m2 <= 59
            && backup.active2 <= 1
            && backup.steps >= (uint32_t)FEED_STEPS_PER_SEC
            && backup.steps <= 600UL * (uint32_t)FEED_STEPS_PER_SEC
            && backup.steps % (uint32_t)FEED_STEPS_PER_SEC == 0
            && cfgChecksum(backup) == backup.crc;
  if (valid) {
    gCfg = backup;
    gCfgFromEeprom = true;
  } else {
    cfgApplyDefaults();
    gCfgFromEeprom = false;
  }
}

// Konfiguration in EEPROM speichern (DS3231 hat kein nutzbares RAM)
static void cfgSaveToEeprom() {
  gCfg.crc = cfgChecksum(gCfg);
  EEPROM.put(EEPROM_CFG_ADDR, gCfg);
  gCfgFromEeprom = true;
}

// Getter/Setter-Impl.
int cfgGetH1() { return gCfg.h1; }
int cfgGetM1() { return gCfg.m1; }
int cfgGetH2() { return gCfg.h2; }
int cfgGetM2() { return gCfg.m2; }
bool cfgGetActive2() { return gCfg.active2 != 0; }
int cfgGetSteps() { return gCfg.steps; }

void cfgUpdateAndSave(uint8_t h1, uint8_t m1, uint8_t h2, uint8_t m2, bool active2, uint32_t steps) {
  // Eingaben begrenzen (Sicherheitsnetz)
  if (h1 > 23) h1 = 23; if (m1 > 59) m1 = 59;
  if (h2 > 23) h2 = 23; if (m2 > 59) m2 = 59;
  if (steps < (uint32_t)FEED_STEPS_PER_SEC) steps = FEED_STEPS_PER_SEC;
  if (steps > 600UL * (uint32_t)FEED_STEPS_PER_SEC) steps = 600UL * FEED_STEPS_PER_SEC;

  gCfg.h1 = h1; gCfg.m1 = m1;
  gCfg.h2 = h2; gCfg.m2 = m2;
  gCfg.active2 = active2 ? 1 : 0;
  gCfg.steps = steps;
  gCfg.crc = cfgChecksum(gCfg);
  cfgApplyToRuntime();
  cfgSaveToEeprom();
}

// Hilfsfunktion twoDigits() entfernt, um String-Allokationen zu vermeiden

static void printStatusStartup() {
  // RTC-Status und Zeit ausgeben (falls gültig), sonst Platzhalter
  char timeBuf[6] = "--:--";
  const char* rtcStat = "--";
  if (rtcIsValid()) {
    DateTime now = rtc.now();
    snprintf(timeBuf, sizeof(timeBuf), "%02u:%02u", now.hour(), now.minute());
    rtcStat = "OK";
  }
  const char* cfgStat = gCfgFromEeprom ? "EEPROM" : "DEF";
  // IP dynamisch ermitteln
  IPAddress ip = WiFi.localIP();
  char ipBuf[24];
  snprintf(ipBuf, sizeof(ipBuf), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
  char line[180];
  snprintf(line, sizeof(line),
           "AP:KORN Pw:Chaosfeeder IP:%s | TIME:%s | Last:--:--(----) | Next1:--:--(--:--) Next2:-- [--] | Steps:%d | RTC:%s CFG:%s | Up:00:00",
           ipBuf, timeBuf, (int)gCfg.steps, rtcStat, cfgStat);
  Serial.println(line);
}

// Hilfsfunktionen für Zeitformatierung und Next/Last
void formatHM(char* buf, size_t len, int h, int m) {
  if (h < 0 || m < 0) { strncpy(buf, "--:--", len); return; }
  snprintf(buf, len, "%02d:%02d", h, m);
}

// Countdown (Differenz von jetzt bis Zielzeit, berücksichtigt Tageswechsel)
static void diffToHM(int nowH, int nowM, int tH, int tM, int &outH, int &outM) {
  if (tH < 0 || tM < 0) { outH = -1; outM = -1; return; }
  int now = nowH * 60 + nowM;
  int tgt = tH * 60 + tM;
  int diff = tgt - now;
  if (diff < 0) diff += 24 * 60; // morgen
  outH = diff / 60;
  outM = diff % 60;
}

// Liefert Momentanwerte für Web-UI
void getStatusSnapshot(bool &rtcOk, int &nowH, int &nowM, int &nowS,
                       int &lastH, int &lastM,
                       int &n1H, int &n1M, int &n2H, int &n2M,
                       int &c1H, int &c1M, int &c2H, int &c2M,
                       bool &a2, int &steps, uint8_t &lastSrc, bool &delayWarning,
                       int &actualH, int &actualM, int &actualS) {
  DateTime nowDt;
  int nowSec = -1;
  rtcOk = getNow(nowDt, nowSec);
  lastH = lastFeedHour; lastM = lastFeedMinute;
  lastSrc = lastFeedSrc;
  a2 = (gCfg.active2 != 0);
  steps = gCfg.steps;
  delayWarning = false;
  actualH = actualM = actualS = -1;
  nowH = nowM = nowS = -1; n1H = n1M = n2H = n2M = -1; c1H = c1M = c2H = c2M = -1;
  if (nowSec >= 0) {
    nowH = nowSec / 3600;
    nowM = (nowSec / 60) % 60;
    nowS = nowSec % 60;
    computeNextTimes(nowH, nowM, n1H, n1M, n2H, n2M);
    diffToHM(nowH, nowM, n1H, n1M, c1H, c1M);
    diffToHM(nowH, nowM, n2H, n2M, c2H, c2M);

    // Prüfen ob nächste Fütterung durch 2-Min-Mindestabstand verzögert wird
    if (lastFeedHour >= 0 && lastFeedMinute >= 0) {
      int lastSec = lastFeedHour * 3600 + lastFeedMinute * 60 + ((lastFeedSecond >= 0) ? lastFeedSecond : 0);
      int deltaLastSec = nowSec - lastSec;
      if (deltaLastSec < 0) deltaLastSec += 24 * 3600; // Mitternacht-Wrap

      // Wenn letzte Fütterung < 2 Min her und nächste Fütterung < 2 Min entfernt
      if (deltaLastSec < (int)MIN_GAP_MIN * 60 && c1H == 0 && c1M < 2) {
        delayWarning = true;
        // Berechne tatsächliche Ausführungszeit: letzte Fütterung + 2 Minuten
        int actualSec = lastSec + (int)MIN_GAP_MIN * 60;
        if (actualSec >= 24 * 3600) actualSec -= 24 * 3600; // Mitternacht-Wrap
        actualH = actualSec / 3600;
        actualM = (actualSec / 60) % 60;
        actualS = actualSec % 60;
      }
    }
  } else {
    // Keine Uhrzeit verfügbar: konfigurierte Zeiten als Referenz anzeigen
    n1H = gCfg.h1; n1M = gCfg.m1;
    if (gCfg.active2) { n2H = gCfg.h2; n2M = gCfg.m2; }
  }
}

static void computeNextTimes(int nowH, int nowM, int &n1H, int &n1M, int &n2H, int &n2M) {
  // Ermittelt die nächsten geplanten Zeiten ab jetzt (heute/ggf. morgen)
  // Kandidatenliste
  int cH[2] = { (int)gCfg.h1, (int)gCfg.h2 };
  int cM[2] = { (int)gCfg.m1, (int)gCfg.m2 };
  bool cActive[2] = { true, gCfg.active2 != 0 };

  // Zeiten, die später als jetzt sind (heute)
  int firstIdx = -1, secondIdx = -1;
  for (int i = 0; i < 2; ++i) {
    if (!cActive[i]) continue;
    if (cH[i] > nowH || (cH[i] == nowH && cM[i] > nowM)) {
      if (firstIdx < 0 || cH[i] < cH[firstIdx] || (cH[i] == cH[firstIdx] && cM[i] < cM[firstIdx])) {
        secondIdx = firstIdx;
        firstIdx = i;
      } else if (secondIdx < 0 || cH[i] < cH[secondIdx] || (cH[i] == cH[secondIdx] && cM[i] < cM[secondIdx])) {
        secondIdx = i;
      }
    }
  }

  if (firstIdx >= 0) { n1H = cH[firstIdx]; n1M = cM[firstIdx]; }
  else { // heute nichts mehr -> morgen erster aktiver Termin
    // nimm den frühesten aktiven am Tag
    int best = -1;
    for (int i = 0; i < 2; ++i) if (cActive[i]) {
      if (best < 0 || cH[i] < cH[best] || (cH[i] == cH[best] && cM[i] < cM[best])) best = i;
    }
    if (best >= 0) { n1H = cH[best]; n1M = cM[best]; } else { n1H = n1M = -1; }
  }

  // Zweiter nächster Termin (falls vorhanden und aktiv)
  if (secondIdx >= 0) { 
    n2H = cH[secondIdx]; n2M = cM[secondIdx]; 
  }
  else if (firstIdx >= 0) {
    // Nur ein Termin heute gefunden -> der andere ist morgen der zweite
    for (int i = 0; i < 2; ++i) {
      if (cActive[i] && i != firstIdx) {
        n2H = cH[i]; n2M = cM[i];
        break;
      }
    }
    if (n2H < 0) n2H = n2M = -1; // Fallback falls nur eine Zeit aktiv
  } else {
    // Heute nichts mehr -> morgen beide Termine, zweiter ist der spätere
    int best1 = -1, best2 = -1;
    for (int i = 0; i < 2; ++i) if (cActive[i]) {
      if (best1 < 0 || cH[i] < cH[best1] || (cH[i] == cH[best1] && cM[i] < cM[best1])) {
        best2 = best1;
        best1 = i;
      } else if (best2 < 0 || cH[i] < cH[best2] || (cH[i] == cH[best2] && cM[i] < cM[best2])) {
        best2 = i;
      }
    }
    if (best2 >= 0) { n2H = cH[best2]; n2M = cM[best2]; } else { n2H = n2M = -1; }
  }
}

static void printStatusPeriodic() {
  unsigned long secs = millis() / 1000UL;
  unsigned int upH = (secs / 3600UL) % 100U;
  unsigned int upM = (secs / 60UL) % 60U;
  char timeBuf[6] = "--:--";
  char lastBuf[6] = "--:--";
  char n1Buf[6] = "--:--";
  char n2Buf[6] = "--:--";
  // Countdowns vorab anlegen (außerhalb des if, damit später nutzbar)
  char c1Buf[6] = "--:--";
  char c2Buf[6] = "--:--";
  char n2Bracket[8] = "--"; // Inhalt in eckigen Klammern für Next2
  DateTime now;
  int nowSec = -1;
  bool rtcOk = getNow(now, nowSec);
  const char* rtcStat = rtcOk ? "OK" : "--";
  if (nowSec >= 0) {
    int nowH = nowSec / 3600;
    int nowM = (nowSec / 60) % 60;
    snprintf(timeBuf, sizeof(timeBuf), "%02u:%02u", nowH, nowM);
    formatHM(lastBuf, sizeof(lastBuf), lastFeedHour, lastFeedMinute);
    int n1H=-1,n1M=-1,n2H=-1,n2M=-1;
    computeNextTimes(nowH, nowM, n1H, n1M, n2H, n2M);
    formatHM(n1Buf, sizeof(n1Buf), n1H, n1M);
    if (gCfg.active2) {
      formatHM(n2Buf, sizeof(n2Buf), n2H, n2M);
    } else {
      strncpy(n2Buf, "--", sizeof(n2Buf));
    }
    // Countdowns berechnen (Differenz bis zur Zielzeit, inkl. Tageswechsel)
    int c1H=-1, c1M=-1, c2H=-1, c2M=-1;
    diffToHM(nowH, nowM, n1H, n1M, c1H, c1M);
    diffToHM(nowH, nowM, n2H, n2M, c2H, c2M);
    if (c1H >= 0) snprintf(c1Buf, sizeof(c1Buf), "%02d:%02d", c1H, c1M);
    if (gCfg.active2 && c2H >= 0) snprintf(c2Buf, sizeof(c2Buf), "%02d:%02d", c2H, c2M);
    // Bracket-Text für Next2
    if (gCfg.active2) {
      snprintf(n2Bracket, sizeof(n2Bracket), "T-%s", c2Buf);
    } else {
      strncpy(n2Bracket, "off", sizeof(n2Bracket));
    }
  }
  const char* cfgStat = gCfgFromEeprom ? "EEPROM" : "DEF";
  char upBuf[6];
  snprintf(upBuf, sizeof(upBuf), "%02u:%02u", upH, upM);
  // IP dynamisch ermitteln
  IPAddress ip = WiFi.localIP();
  char ipBuf[24];
  snprintf(ipBuf, sizeof(ipBuf), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
  char line[220];
  // Countdowns: c1Buf/c2Buf wurden oben gesetzt (oder bleiben --:--)
  char lastStepsBuf[8];
  if (lastFeedSteps >= 0) snprintf(lastStepsBuf, sizeof(lastStepsBuf), "%d", lastFeedSteps); else strncpy(lastStepsBuf, "----", sizeof(lastStepsBuf));
  snprintf(line, sizeof(line),
           "AP:KORN Pw:Chaosfeeder IP:%s | TIME:%s | Last:%s(%s) | Next1:%s(T-%s) Next2:%s [%s] | Steps:%d | RTC:%s CFG:%s | Up:%s",
           ipBuf, timeBuf, lastBuf, lastStepsBuf, n1Buf, c1Buf, n2Buf, n2Bracket, (int)gCfg.steps, rtcStat, cfgStat, upBuf);
  Serial.println(line);
}

// ============================================================================
// RTC-Zeit bei Upload setzen (DS3231)
// ============================================================================
static void rtcInitAndMaybeSet() {
  // Initialisierung der DS3231 (I2C) via RTClib
  gRtcPresent = rtc.begin();
  if (!gRtcPresent) {
    Serial.println(F("FEHLER: DS3231 nicht gefunden (I2C: SDA=A4, SCL=A5 pruefen)"));
    // Trotzdem millis()-Fallback aktivieren, damit Anzeige/Scheduler laeuft
    lastNowSec = 0;
    lastNowMs = millis();
    return;
  }
  // Bei Stromverlust / leerer Stützbatterie -> Uhr auf Kompilierzeit setzen
  gRtcPowerLossDetected = rtc.lostPower();
  if (gRtcPowerLossDetected || !rtcIsValid()) {
    rtcApplyTime(DateTime(__DATE__, __TIME__));
  }
}

// ============================================================================
// Setup / Loop
// ============================================================================
void setup() {
  Serial.begin(115200);
  
  // Watchdog TEMPORÄR DEAKTIVIERT ZUM DEBUGGEN
  #ifdef WATCHDOG_ENABLED
    // wdt_enable(WDTO_8S);  // AUSKOMMENTIERT ZUM TESTEN
    Serial.println(F("Watchdog: DEAKTIVIERT (Debug-Modus)"));
  #else
    Serial.println(F("Watchdog: nicht verfügbar (UNO R4)"));
  #endif

  rtcInitAndMaybeSet();

  // Konfiguration aus EEPROM laden (oder Defaults)
  cfgLoadFromEeprom();
  cfgApplyToRuntime();

  motorInit();         // Pins, Treiber in sicheren Zustand, ggf. AccelStepper init
  apInit("KORN", "Chaosfeeder"); // AP-Insellösung, feste IP folgt in ap.ino
  webInit();

  // Manuelle Auslösung vorbereiten
  pinMode(MANUAL_TRIGGER_PIN, INPUT_PULLUP);
  pinMode(MANUAL_GROUND_PIN, OUTPUT);
  digitalWrite(MANUAL_GROUND_PIN, LOW);

  printStatusStartup();
  lastStatusMs = millis();
  bootMs = lastStatusMs;

  // Tageswechsel-Erkennung initialisieren
  if (rtcIsValid()) {
    lastKnownDay = rtc.now().day();
  }
}

void loop() {
  // Watchdog zurücksetzen (verhindert Reset bei normaler Operation)
  #ifdef WATCHDOG_ENABLED
    wdt_reset();
  #endif
  
  // WLAN-Status prüfen und ggf. reconnecten (nur wenn aktiviert)
  if (WIFI_CHECK_INTERVAL_MS > 0) {
    unsigned long nowMs = millis();
    if (nowMs - lastWifiCheckMs >= WIFI_CHECK_INTERVAL_MS) {
      apCheckAndReconnect();
      lastWifiCheckMs = nowMs;
    }
  }

  // Webrequests bedienen
  webHandleClient();

  // Periodische Minimal-Ausgabe
  const unsigned long intervalMs = (unsigned long)STATUS_INTERVAL_S * 1000UL;
  unsigned long now = millis();
  if (now - lastStatusMs >= intervalMs) {
    printStatusPeriodic();
    lastStatusMs = now;
  }

  // Web-Feed-Request asynchron bedienen (antwortet HTTP sofort; Feed läuft hier)
  if (webFeedRequested && !feedingInProgress) {
    webFeedRequested = false;
    Serial.println(F("TRIG:WEB"));
    nextFeedSrc = SRC_WEB;
    requestImmediateFeed();
  }

  // Hardware-Trigger: Kurzschluss 10↔11 löst aus – robust mit 150ms Hold + 1s Lockout, one-shot bis Release
  int trig = digitalRead(MANUAL_TRIGGER_PIN); // HIGH=Ruhe (Pullup), LOW=kurzgeschlossen
  if (trig == LOW) {
    if (manualLowSinceMs == 0) manualLowSinceMs = now; // Start der Low-Phase
    // Nur auslösen, wenn stabil ≥150ms LOW, kein Lockout aktiv und noch nicht verbraucht
    if (!manualConsumed && (now - manualLowSinceMs) >= 150UL && (long)(now - manualLockoutUntilMs) >= 0 && !feedingInProgress) {
      Serial.println(F("TRIG:MANUAL"));
      nextFeedSrc = SRC_MANUAL;
      requestImmediateFeed();
      manualLockoutUntilMs = now + 1000UL; // 1s Lockout
      manualConsumed = true;               // bis Release gesperrt
      // lowSince nicht zurücksetzen – verhindert Mehrfachauslösung solange gedrückt
    }
  } else {
    // HIGH = Ruhe
    manualLowSinceMs = 0;
    manualConsumed = false; // Release: nächster Druck erlaubt
  }

  // Geplante Fütterungen und Tagesreset
  {
    DateTime nowRtc;
    int nowSec;
    bool rtcOk = getNow(nowRtc, nowSec);
    if (nowSec < 0) {
      Serial.println(F("RTC: Keine Zeit verfuegbar - Scheduler uebersprungen"));
      return;
    }
    unsigned long nowMs = millis();

    // Erste Initialisierung
    if (lastNowSec < 0) {
      lastNowSec = nowSec;
      lastNowMs = nowMs;
    }

    // Tageswechsel erkennen → Marker zurücksetzen
    if (rtcOk && lastKnownDay != nowRtc.day()) {
      fedToday1 = false;
      fedToday2 = false;
      pendingSched1 = false;
      pendingSched2 = false;
      lastNowSec = nowSec;
      lastNowMs = nowMs;
    } else if (!rtcOk && nowSec < lastNowSec && lastNowSec >= 86300 && nowSec <= 120) {
      // Fallback-Mitternacht wenn RTC nicht verfuegbar
      Serial.println(F("RTC: Mitternacht via Fallback erkannt"));
      fedToday1 = false;
      fedToday2 = false;
      pendingSched1 = false;
      pendingSched2 = false;
      lastNowSec = nowSec;
      lastNowMs = nowMs;
    }
    if (rtcOk) {
      lastKnownDay = nowRtc.day();
    }

    // Zeit-Sprung-Detektion (z.B. RTC-Reset, Batterieprobleme oder manuelles Setzen)
    // Wenn die RTC-Zeit nicht mit der verstrichenen Echtzeit übereinstimmt, ignorieren
    // wir diesen Loop und setzen die Referenz neu (verhindert Fütterungen ohne Grund).
    long expectedSecDiff = (long)((nowMs - lastNowMs) / 1000UL) + 3; // +3 Sekunden Toleranz
    int secDiff = (nowSec - lastNowSec + 86400) % 86400;
    if (secDiff > expectedSecDiff) {
      Serial.println(F("RTC: Zeit sprang - Scheduler reset"));
      lastNowSec = nowSec;
      lastNowMs = nowMs;
      return;
    }

    // Zielzeiten (Sekunden)
    int t1Sec = ((int)gCfg.h1) * 3600 + ((int)gCfg.m1) * 60; // :00 Sekunden
    int t2Sec = ((int)gCfg.h2) * 3600 + ((int)gCfg.m2) * 60; // :00 Sekunden
    // Crossing-Detektion mit Wrap-around über Mitternacht:
    // trifft zu, wenn t im Intervall (lastNowSec, nowSec] liegt – auch wenn nowSec < lastNowSec
    auto crossed = [](int lastS, int nowS, int tS) {
      if (lastS <= nowS) {
        return (tS > lastS) && (tS <= nowS);
      } else { // Wrap: z.B. 86390 -> 5
        return (tS > lastS) || (tS <= nowS);
      }
    };
    bool crossed1 = crossed(lastNowSec, nowSec, t1Sec);
    bool crossed2 = crossed(lastNowSec, nowSec, t2Sec);
    bool armed = (millis() - bootMs) >= ARMING_WINDOW_MS;

    if (!feedingInProgress && armed) {
      // Mindestabstand zur letzten Fütterung (sekundengenau, wrap-aware)
      int deltaLastSec = 999999; // groß = "kein Limit"
      if (lastFeedHour >= 0 && lastFeedMinute >= 0) {
        int lastS = lastFeedHour * 3600 + lastFeedMinute * 60 + ((lastFeedSecond >= 0) ? lastFeedSecond : 0);
        deltaLastSec = nowSec - lastS; if (deltaLastSec < 0) deltaLastSec += 24 * 3600;
      }

      bool didTrigger = false;

      // Termin 1: Crossing erkannt UND aktuell exakt :00 Sekunden -> auslösen oder defer
      if (!fedToday1 && crossed1 && (nowSec % 60) == 0) {
        if (deltaLastSec >= (int)MIN_GAP_MIN * 60) {
          // Log mit Sollzeit & aktueller RTC-Zeit
          int curH = nowSec / 3600; int curM = (nowSec / 60) % 60; int curS = nowSec % 60;
          Serial.print(F("TRIG:SCHED1@"));
          if (curH < 10) Serial.print('0'); Serial.print(curH); Serial.print(':');
          if (curM < 10) Serial.print('0'); Serial.print(curM); Serial.print(':');
          if (curS < 10) Serial.print('0'); Serial.println(curS);
          nextFeedSrc = SRC_SCHED1;
          if (requestImmediateFeed()) {
            fedToday1 = true;
            pendingSched1 = false;
            didTrigger = true;
          }
        } else {
          pendingSched1 = true; // nach Gap nachholen
          int curH = nowSec / 3600; int curM = (nowSec / 60) % 60; int curS = nowSec % 60;
          Serial.print(F("DEF:SCHED1(gap)@"));
          if (curH < 10) Serial.print('0'); Serial.print(curH); Serial.print(':');
          if (curM < 10) Serial.print('0'); Serial.print(curM); Serial.print(':');
          if (curS < 10) Serial.print('0'); Serial.println(curS);
        }
      } else if (!fedToday1 && crossed1) {
        // Crossing erkannt, aber nicht bei :00 -> als pending merken
        pendingSched1 = true;
      }

      // Nur eine geplante Fütterung pro Loop-Iteration durchführen
      if (!didTrigger && gCfg.active2 && !fedToday2) {
        // Mindestabstand erneut prüfen (nach evtl. Feed1 wurde lastFeed* aktualisiert)
        int d2Sec = 999999;
        if (lastFeedHour >= 0 && lastFeedMinute >= 0) {
          int lastS = lastFeedHour * 3600 + lastFeedMinute * 60 + ((lastFeedSecond >= 0) ? lastFeedSecond : 0);
          d2Sec = nowSec - lastS; if (d2Sec < 0) d2Sec += 24 * 3600;
        }
        if (crossed2 && (nowSec % 60) == 0) {
          if (d2Sec >= (int)MIN_GAP_MIN * 60) {
            int curH = nowSec / 3600; int curM = (nowSec / 60) % 60; int curS = nowSec % 60;
            Serial.print(F("TRIG:SCHED2@"));
            if (curH < 10) Serial.print('0'); Serial.print(curH); Serial.print(':');
            if (curM < 10) Serial.print('0'); Serial.print(curM); Serial.print(':');
            if (curS < 10) Serial.print('0'); Serial.println(curS);
            nextFeedSrc = SRC_SCHED2;
            if (requestImmediateFeed()) {
              fedToday2 = true;
              pendingSched2 = false;
              didTrigger = true;
            }
          } else {
            pendingSched2 = true; // nach Gap nachholen
            int curH = nowSec / 3600; int curM = (nowSec / 60) % 60; int curS = nowSec % 60;
            Serial.print(F("DEF:SCHED2(gap)@"));
            if (curH < 10) Serial.print('0'); Serial.print(curH); Serial.print(':');
            if (curM < 10) Serial.print('0'); Serial.print(curM); Serial.print(':');
            if (curS < 10) Serial.print('0'); Serial.println(curS);
          }
        } else if (gCfg.active2 && !fedToday2 && crossed2) {
          // Crossing erkannt, aber nicht bei :00 -> als pending merken
          pendingSched2 = true;
        }
      }

      // Nachholer für pending Termine (auch wenn :00 verpasst wurde)
      if (!didTrigger) {
        if (pendingSched1 && !fedToday1 && deltaLastSec >= (int)MIN_GAP_MIN * 60) {
          Serial.println(F("TRIG:SCHED1(pending)"));
          nextFeedSrc = SRC_SCHED1;
          if (requestImmediateFeed()) {
            fedToday1 = true;
            pendingSched1 = false;
            didTrigger = true;
          }
        }
      }
      if (!didTrigger) {
        if (pendingSched2 && gCfg.active2 && !fedToday2) {
          // Mindestabstand prüfen (sekundengenau)
          int dSec = 999999;
          if (lastFeedHour >= 0 && lastFeedMinute >= 0) {
            int lastS = lastFeedHour * 3600 + lastFeedMinute * 60 + ((lastFeedSecond >= 0) ? lastFeedSecond : 0);
            dSec = nowSec - lastS; if (dSec < 0) dSec += 24 * 3600;
          }
          if (dSec >= (int)MIN_GAP_MIN * 60) {
            Serial.println(F("TRIG:SCHED2(pending)"));
            nextFeedSrc = SRC_SCHED2;
            if (requestImmediateFeed()) {
              fedToday2 = true;
              pendingSched2 = false;
            }
          }
        }
      }
      
      // Direkter Fallback: Wenn Crossing vor >5s war und noch nicht gefüttert, sofort triggern
      if (!didTrigger && !fedToday1 && crossed1 && deltaLastSec >= (int)MIN_GAP_MIN * 60) {
        int secsSinceCrossing = nowSec - t1Sec; if (secsSinceCrossing < 0) secsSinceCrossing += 24 * 3600;
        if (secsSinceCrossing > 0 && secsSinceCrossing <= 10) {
          Serial.println(F("TRIG:SCHED1(late)"));
          nextFeedSrc = SRC_SCHED1;
          if (requestImmediateFeed()) {
            fedToday1 = true;
            pendingSched1 = false;
            didTrigger = true;
          }
        }
      }
      if (!didTrigger && gCfg.active2 && !fedToday2 && crossed2) {
        int dSec = 999999;
        if (lastFeedHour >= 0 && lastFeedMinute >= 0) {
          int lastS = lastFeedHour * 3600 + lastFeedMinute * 60 + ((lastFeedSecond >= 0) ? lastFeedSecond : 0);
          dSec = nowSec - lastS; if (dSec < 0) dSec += 24 * 3600;
        }
        if (dSec >= (int)MIN_GAP_MIN * 60) {
          int secsSinceCrossing = nowSec - t2Sec; if (secsSinceCrossing < 0) secsSinceCrossing += 24 * 3600;
          if (secsSinceCrossing > 0 && secsSinceCrossing <= 10) {
            Serial.println(F("TRIG:SCHED2(late)"));
            nextFeedSrc = SRC_SCHED2;
            if (requestImmediateFeed()) {
              fedToday2 = true;
              pendingSched2 = false;
            }
          }
        }
      }
    } else if (feedingInProgress) {
      // Während Fütterung merken wir Crossings in diesem Intervall als pending
      if (!fedToday1 && crossed1) pendingSched1 = true;
      if (gCfg.active2 && !fedToday2 && crossed2) pendingSched2 = true;
    } else if (!armed) {
      // Noch nicht armed: Crossings als pending merken, damit direkt nach Arming ausgelöst werden
      if (!fedToday1 && crossed1) pendingSched1 = true;
      if (gCfg.active2 && !fedToday2 && crossed2) pendingSched2 = true;
    }
    // Aktuelle Sekunde als Referenz für das nächste Intervall behalten
    lastNowSec = nowSec;
    lastNowMs = nowMs;
  }
}

// ============================================================================
// Zentrale Sofortfütterung – aktualisiert Status und sperrt Reentrancy
// ============================================================================
bool requestImmediateFeed() {
  if (feedingInProgress) return false;
  feedingInProgress = true;

  // Aktuelle Zeit bestimmen (RTC oder millis()-Fallback)
  DateTime nowDt;
  int nowSec = -1;
  bool rtcOk = getNow(nowDt, nowSec);

  // Motorfahrt ausführen (Jagdsignal "Zum Essen" abspielen)
  bool success = motorFeed(gCfg.steps, MOTOR_DIR_CW, true);

  if (success) {
    // Last Feed Zeit übernehmen (falls eine Zeit verfügbar ist)
    if (nowSec >= 0) {
      lastFeedHour = nowSec / 3600;
      lastFeedMinute = (nowSec / 60) % 60;
      lastFeedSecond = nowSec % 60;
    }
    if (rtcOk) {
      lastKnownDay = nowDt.day(); // sicherstellen
    }
    // Letzte Schritte merken
    lastFeedSteps = gCfg.steps;

    // Quelle der letzten Fütterung merken und zurücksetzen
    lastFeedSrc = nextFeedSrc;
    nextFeedSrc = SRC_NONE;

    // Optional: Stepper-Reset-Hook
    motorReset();
  }

  feedingInProgress = false;
  if (!success) {
    nextFeedSrc = SRC_NONE;
  }
  return success;
}
