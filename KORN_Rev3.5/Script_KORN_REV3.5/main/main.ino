/*
 * K.O.R.N. Rev3.5 – Modularer Sketch
 * Dateien:
 *  - main.ino          (Setup/Loop, Orchestrierung, Timer/Status)
 *  - motor.ino         (Motorsteuerung/Relais/ENA)
 *  - wifi_client.ino   (WLAN-Client + Ersteinrichtungs-AP-Fallback)
 *  - homepage.ino      (Webserver & Routen)
 *
 * Hardware (README):
 *  - Motor:  PUL=D2, DIR=D3, ENA=D5, Relais=D8
 *  - Buzzer: D6 aktiv (HIGH=Ton)
 *  - Manuell: D10 (INPUT_PULLUP), D11 (OUTPUT LOW) -> Kurzschluss löst aus
 *  - DS1302 entfällt: Zeitsteuerung über NTP
 *
 * Benötigte Bibliotheken (Arduino IDE Library Manager):
 *  - "AccelStepper" Version 1.61+       (für Schrittmotor-Steuerung)
 *  - WiFiS3 (vorinstalliert)             (für Arduino UNO R4 WiFi)
 *  - WiFiUdp (vorinstalliert)            (für NTP)
 *  - EEPROM (vorinstalliert)             (für Konfiguration + WLAN-Zugangsdaten)
 *  - avr/wdt.h (nur AVR, vorinstalliert) (für Watchdog, nicht auf UNO R4)
 *
 * Board: Arduino UNO R4 WiFi
 * Board Package: Arduino UNO R4 Boards (arduino:renesas_uno)
 */

#include <Arduino.h>
#include <EEPROM.h>     // Konfiguration + WLAN-Zugangsdaten
#include <WiFiUdp.h>
// Watchdog nur für AVR-basierte Arduinos (UNO R4 nutzt anderen Mechanismus)
#ifdef __AVR__
  #include <avr/wdt.h>
  #define WATCHDOG_ENABLED
#endif
#include <AccelStepper.h>
#include <WiFiS3.h>

// Vorwärtsdeklaration: nötig, damit Arduino-Präprozessor-Prototypen KConfig kennen
struct KConfig;

// WLAN-Zugangsdaten Struktur (genutzt von wifi_client.ino)
#define WIFI_CRED_MAGIC 0x4B57
struct WifiCred {
  uint16_t magic;
  char ssid[33];
  char pass[64];
};

// Globale Serverinstanz (Port 80) – genutzt von wifi_client.ino und homepage.ino
WiFiServer server(80);

// ============================================================================
// Defaults & Flags (aus README)
// ============================================================================
const int FUETTERUNG_STUNDE_1 = 7;
const int FUETTERUNG_MINUTE_1 = 1;
const int FUETTERUNG_STUNDE_2 = 16;
const int FUETTERUNG_MINUTE_2 = 1;
bool ZWEITE_ZEIT_AKTIV = true;          // zweite Zeit aktiv
const int FEED_STEPS_PER_SEC = 1000;    // feste Schrittfrequenz für zeitbasierte Fütterung
const int MOTOR_SCHRITTE = 2000;        // legacy: wird aus Sekunden berechnet (secs*FEED_STEPS_PER_SEC)
const bool MOTOR_DIR_CW = false;        // false = CCW (links), true = CW (rechts)

// Serielle Minimal-Ausgabe Intervall (Sekunden)
const uint16_t STATUS_INTERVAL_S = 60;
// Mindestabstand zwischen zwei Fütterungen (Minuten) – schützt vor Doppel-Triggern
const uint16_t MIN_GAP_MIN = 2;
// Arming-Window nach Boot: geplante Fütterungen erst nach diesem Zeitfenster erlauben
const uint32_t ARMING_WINDOW_MS = 60000UL;
// WLAN-Reconnect Intervall (Millisekunden)
const uint32_t WIFI_CHECK_INTERVAL_MS = 30000UL;

// NTP-Einstellungen
#define NTP_SERVER      "pool.ntp.org"
#define NTP_TIMEZONE_H  1    // UTC+1 (Winterzeit)
#define NTP_DST_H       1    // +1 Stunde Sommerzeit
#define NTP_SYNC_INTERVAL_MS  3600000UL  // NTP-Resync alle 60 Minuten

// NTP-Zustand
static WiFiUDP ntpUDP;
static bool gNtpSynced = false;
static unsigned long gNtpLastSyncMs = 0;
static int gNtpHour = -1, gNtpMin = -1, gNtpSec = -1;
static unsigned long gNtpBaseMs = 0;  // millis() zum Zeitpunkt des letzten Syncs
static uint8_t gNtpDay = 0;           // Tag des letzten Syncs (für Mitternachts-Reset)

// Quelle der letzten/kommenden Fütterung (für Status/Anzeige)
const uint8_t SRC_NONE = 0;
const uint8_t SRC_MANUAL = 1;
const uint8_t SRC_WEB = 2;
const uint8_t SRC_SCHED1 = 3;
const uint8_t SRC_SCHED2 = 4;


// Manuelle Auslösung (Kurzschluss 10↔11)
#define MANUAL_TRIGGER_PIN 10  // INPUT_PULLUP
#define MANUAL_GROUND_PIN 11   // OUTPUT LOW

// Vorwärtsdeklarationen der Modul-APIs
void motorInit();
void motorEnable(bool on);
void motorReset();
bool motorFeed(int steps, bool dirCW);

void wifiInit();
void wifiCheckAndReconnect();
void dnsHandleRequests();
bool wifiIsSetupMode();
bool wifiIsConnected();
const char* wifiGetSSID();
void wifiCredSave(const char* ssid, const char* pass);
void webInit();
void webHandleClient();

// Kompakter Status-Snapshot für die Web-UI (RAM-schonend, keine Strings)
void getStatusSnapshot(bool &ntpOk, int &nowH, int &nowM, int &nowS,
                       int &lastH, int &lastM,
                       int &n1H, int &n1M, int &n2H, int &n2M,
                       int &c1H, int &c1M, int &c2H, int &c2M,
                       bool &a2, int &steps, uint8_t &lastSrc, bool &delayWarning,
                       int &actualH, int &actualM, int &actualS);

// Hilfsfunktion zur Berechnung der nächsten Fütterungszeiten
static void computeNextTimes(int nowH, int nowM, int &n1H, int &n1M, int &n2H, int &n2M);

// Sofortfütterung zentral aus main anstoßen (für Web/Hardware)
void requestImmediateFeed();

// Konfig-Getter/Setter für Web-UI (entkoppelt von struct in anderen Modulen)
int cfgGetH1();
int cfgGetM1();
int cfgGetH2();
int cfgGetM2();
bool cfgGetActive2();
int cfgGetSteps();
void cfgUpdateAndSave(uint8_t h1, uint8_t m1, uint8_t h2, uint8_t m2, bool active2, uint32_t steps);
// Batterie-Indikator (softwarebasiert): RTC-Zeit gültig und Config aus RTC-RAM
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
// firstSchedCheck entfernt – Catch-up nur via pending

// =========================================================================
// Konfiguration (Zeiten, Aktiv-Flag, Steps) – Ziel: DS1302-RAM (Persistenz)
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
static bool gCfgValid = false; // Anzeige für Statuszeile CFG:OK/--
static bool gCfgFromRam = false; // Herkunft: true=aus RTC-RAM, false=Defaults
static bool gCfgFromEeprom = false; // Herkunft: true=aus EEPROM geladen, false=nicht aus EEPROM

// EEPROM-Speicheradresse für Konfiguration
#define EEPROM_CFG_ADDR 0

bool ntpIsSynced() {
  return gNtpSynced;
}

static uint16_t cfgChecksum(const KConfig &c) {
  // Sehr einfache Prüfsumme über die payload-Felder (ohne magic, v, crc)
  uint32_t s = 0;
  s += c.h1 + c.m1 + c.h2 + c.m2 + c.active2;
  s += c.steps & 0xFF; s += (c.steps >> 8) & 0xFF; s += (c.steps >> 16) & 0xFF; s += (c.steps >> 24) & 0xFF;
  return (uint16_t)((s & 0xFFFFu) ^ 0xA5A5u);
}

// NTP-Paket senden und Antwort auswerten
static bool ntpSync() {
  const char* ntpServer = NTP_SERVER;
  const int NTP_PACKET_SIZE = 48;
  byte packetBuffer[NTP_PACKET_SIZE];

  // NTP-Paket vorbereiten
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  packetBuffer[0]  = 0b11100011; // LI, Version, Mode
  packetBuffer[1]  = 0;          // Stratum
  packetBuffer[2]  = 6;          // Polling Interval
  packetBuffer[3]  = 0xEC;       // Peer Clock Precision
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;

  ntpUDP.begin(2390);
  ntpUDP.beginPacket(ntpServer, 123);
  ntpUDP.write(packetBuffer, NTP_PACKET_SIZE);
  ntpUDP.endPacket();

  // Auf Antwort warten (max. 2s)
  uint32_t start = millis();
  while (millis() - start < 2000) {
    if (ntpUDP.parsePacket() >= NTP_PACKET_SIZE) {
      ntpUDP.read(packetBuffer, NTP_PACKET_SIZE);
      // Sekunden seit 1900
      uint32_t secsSince1900 = ((uint32_t)packetBuffer[40] << 24) |
                               ((uint32_t)packetBuffer[41] << 16) |
                               ((uint32_t)packetBuffer[42] <<  8) |
                                (uint32_t)packetBuffer[43];
      // Unix-Zeit (seit 1970)
      const uint32_t SEVENTY_YEARS = 2208988800UL;
      uint32_t epoch = secsSince1900 - SEVENTY_YEARS;

      // Zeitzone + Sommerzeit (einfache Heuristik: März–Oktober = Sommerzeit)
      uint32_t localEpoch = epoch + (uint32_t)(NTP_TIMEZONE_H + ntpIsDST(epoch)) * 3600UL;

      // Uhrzeit extrahieren
      gNtpHour = (localEpoch % 86400UL) / 3600;
      gNtpMin  = (localEpoch % 3600UL)  / 60;
      gNtpSec  = localEpoch % 60;
      gNtpDay  = (uint8_t)((localEpoch / 86400UL) % 31); // Tages-ID für Reset
      gNtpBaseMs = millis();
      gNtpSynced = true;
      ntpUDP.stop();
      Serial.print(F("NTP sync: "));
      Serial.print(gNtpHour); Serial.print(':');
      if (gNtpMin < 10) Serial.print('0'); Serial.print(gNtpMin); Serial.print(':');
      if (gNtpSec < 10) Serial.print('0'); Serial.println(gNtpSec);
      return true;
    }
    delay(10);
  }
  ntpUDP.stop();
  Serial.println(F("NTP sync fehlgeschlagen."));
  return false;
}

// Einfache Sommerzeit-Heuristik (Europa): März letzter Sonntag – Oktober letzter Sonntag
static int ntpIsDST(uint32_t epoch) {
  uint32_t days = epoch / 86400UL;
  uint16_t year = 1970;
  uint32_t d = days;
  while (true) {
    uint32_t y = (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)) ? 366 : 365;
    if (d < y) break;
    d -= y; year++;
  }
  // Monat berechnen
  uint8_t month = 1;
  uint8_t mdays[] = {31,28,31,30,31,30,31,31,30,31,30,31};
  if (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)) mdays[1] = 29;
  for (int i = 0; i < 12 && d >= mdays[i]; i++) { d -= mdays[i]; month++; }
  if (month < 3 || month > 10) return 0;
  if (month > 3 && month < 10) return 1;
  // März/Oktober: letzter Sonntag
  // Vereinfachung: ab Tag 25 im Monat als DST-Grenze
  if (month == 3) return (d >= 24) ? 1 : 0;
  if (month == 10) return (d >= 24) ? 0 : 1;
  return 0;
}

// Aktuelle Zeit berechnen (NTP-Basis + millis-Drift)
static void ntpGetTime(int &h, int &m, int &s) {
  if (!gNtpSynced) { h = -1; m = -1; s = -1; return; }
  uint32_t elapsed = (millis() - gNtpBaseMs) / 1000UL;
  uint32_t totalSec = (uint32_t)(gNtpHour * 3600 + gNtpMin * 60 + gNtpSec) + elapsed;
  totalSec %= 86400UL; // Mitternacht-Wrap
  h = totalSec / 3600;
  m = (totalSec % 3600) / 60;
  s = totalSec % 60;
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
  gCfgValid = true; // defaults gelten als gültig
}

static void cfgApplyToRuntime() {
  // Setzt die Laufzeit-Parameter aus gCfg
  // Hinweis: Konstanten FUETTERUNG_* sind Defaults; Laufzeit nutzt gCfg.
  // Für einfache Integration verwenden wir gCfg direkt in Berechnungen.
}

// Vorwärtsdeklaration für cfgSaveToRtcRam (wird in cfgLoadFromRtcRam verwendet)
static void cfgSaveToRtcRam();

// EEPROM-Funktionen für Backup-Persistenz
static bool cfgLoadFromEEPROM() {
  KConfig backup;
  EEPROM.get(EEPROM_CFG_ADDR, backup);
  if (backup.magic == 0xBEEF) {
    uint16_t c = cfgChecksum(backup);
    if (c == backup.crc) {
      gCfg = backup;
      gCfgValid = true;
      gCfgFromEeprom = true;
      return true;
    }
  }
  return false;
}

static void cfgSaveToEEPROM() {
  gCfg.crc = cfgChecksum(gCfg);
  EEPROM.put(EEPROM_CFG_ADDR, gCfg);
}

// Persistenz in DS1302-RAM – Makuna-API: wird separat verifiziert.
// Platzhalter, damit Web-Konfig bereits funktioniert (CFG-Status zeigt Gültigkeit an).
static void cfgLoadFromRtcRam() {
  // Rev3.5: kein DS1302-RAM mehr – direkt aus EEPROM laden
  if (cfgLoadFromEEPROM()) {
    gCfgFromRam = false;
    Serial.println(F("Config aus EEPROM geladen."));
  } else {
    cfgApplyDefaults();
    gCfgFromRam = false;
    gCfgFromEeprom = false;
    Serial.println(F("Config: Standardwerte."));
  }
}

static void cfgSaveToRtcRam() {
  // Rev3.5: nur noch EEPROM
  cfgSaveToEEPROM();
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
  if (steps < 1) steps = 1;

  gCfg.h1 = h1; gCfg.m1 = m1;
  gCfg.h2 = h2; gCfg.m2 = m2;
  gCfg.active2 = active2 ? 1 : 0;
  gCfg.steps = steps;
  gCfg.crc = cfgChecksum(gCfg);
  gCfgValid = true;
  cfgApplyToRuntime();
  cfgSaveToRtcRam();
}

// Hilfsfunktion twoDigits() entfernt, um String-Allokationen zu vermeiden

static const char* SETUP_AP_SSID_STR = "KORN-Setup";

static void printStatusStartup() {
  char timeBuf[6] = "--:--";
  const char* ntpStat = gNtpSynced ? "OK" : "--";
  if (gNtpSynced) {
    int h, m, s; ntpGetTime(h, m, s);
    snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", h, m);
  }
  const char* cfgStat = gCfgFromEeprom ? "EEPROM" : "DEF";
  // IP dynamisch ermitteln
  IPAddress ip = WiFi.localIP();
  char ipBuf[24];
  snprintf(ipBuf, sizeof(ipBuf), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
  char line[180];
  snprintf(line, sizeof(line),
           "SSID:%s IP:%s | TIME:%s | Last:--:--(----) | Next1:--:--(--:--) Next2:-- [--] | Steps:%d | NTP:%s CFG:%s | Up:00:00",
           wifiIsSetupMode() ? SETUP_AP_SSID_STR : wifiGetSSID(), ipBuf, timeBuf, (int)gCfg.steps, ntpStat, cfgStat);
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
void getStatusSnapshot(bool &ntpOk, int &nowH, int &nowM, int &nowS,
                       int &lastH, int &lastM,
                       int &n1H, int &n1M, int &n2H, int &n2M,
                       int &c1H, int &c1M, int &c2H, int &c2M,
                       bool &a2, int &steps, uint8_t &lastSrc, bool &delayWarning,
                       int &actualH, int &actualM, int &actualS) {
  ntpOk = gNtpSynced;
  lastH = lastFeedHour; lastM = lastFeedMinute;
  lastSrc = lastFeedSrc;
  a2 = (gCfg.active2 != 0);
  steps = gCfg.steps;
  delayWarning = false;
  actualH = actualM = actualS = -1;
  nowH = nowM = nowS = -1; n1H = n1M = n2H = n2M = -1; c1H = c1M = c2H = c2M = -1;
  if (ntpOk) {
    ntpGetTime(nowH, nowM, nowS);
    if (nowH < 0) { ntpOk = false; return; }
    computeNextTimes(nowH, nowM, n1H, n1M, n2H, n2M);
    diffToHM(nowH, nowM, n1H, n1M, c1H, c1M);
    diffToHM(nowH, nowM, n2H, n2M, c2H, c2M);
    
    // Prüfen ob nächste Fütterung durch 2-Min-Mindestabstand verzögert wird
    if (lastFeedHour >= 0 && lastFeedMinute >= 0) {
      int nowSec = nowH * 3600 + nowM * 60 + nowS;
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
  const char* ntpStat = gNtpSynced ? "OK" : "--";
  char c1Buf[6] = "--:--";
  char c2Buf[6] = "--:--";
  char n2Bracket[8] = "--";
  int ntpH = -1, ntpM = -1, ntpS = -1;
  ntpGetTime(ntpH, ntpM, ntpS);
  if (gNtpSynced && ntpH >= 0) {
    snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", ntpH, ntpM);
    formatHM(lastBuf, sizeof(lastBuf), lastFeedHour, lastFeedMinute);
    int n1H=-1,n1M=-1,n2H=-1,n2M=-1;
    computeNextTimes(ntpH, ntpM, n1H, n1M, n2H, n2M);
    formatHM(n1Buf, sizeof(n1Buf), n1H, n1M);
    if (gCfg.active2) {
      formatHM(n2Buf, sizeof(n2Buf), n2H, n2M);
    } else {
      strncpy(n2Buf, "--", sizeof(n2Buf));
    }
    // Countdowns berechnen (Differenz bis zur Zielzeit, inkl. Tageswechsel)
    int c1H=-1, c1M=-1, c2H=-1, c2M=-1;
    diffToHM(ntpH, ntpM, n1H, n1M, c1H, c1M);
    diffToHM(ntpH, ntpM, n2H, n2M, c2H, c2M);
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
  IPAddress ip = WiFi.localIP();
  char ipBuf[24];
  snprintf(ipBuf, sizeof(ipBuf), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
  char line[220];
  char lastStepsBuf[8];
  if (lastFeedSteps >= 0) snprintf(lastStepsBuf, sizeof(lastStepsBuf), "%d", lastFeedSteps); else strncpy(lastStepsBuf, "----", sizeof(lastStepsBuf));
  snprintf(line, sizeof(line),
           "SSID:%s IP:%s | TIME:%s | Last:%s(%s) | Next1:%s(T-%s) Next2:%s [%s] | Steps:%d | NTP:%s CFG:%s | Up:%s",
           wifiIsSetupMode() ? "KORN-Setup" : wifiGetSSID(), ipBuf, timeBuf, lastBuf, lastStepsBuf, n1Buf, c1Buf, n2Buf, n2Bracket, (int)gCfg.steps, gNtpSynced ? "OK" : "--", cfgStat, upBuf);
  Serial.println(line);
}

// NTP-Sync versuchen (nur wenn WLAN verbunden und nicht im Setup-Modus)
static void ntpTrySync() {
  if (!wifiIsConnected() || wifiIsSetupMode()) return;
  ntpSync();
}

// ============================================================================
// Setup / Loop
// ============================================================================
void setup() {
  Serial.begin(115200);
  // Auf Serial warten, aber max. 2 Sekunden – sonst hängt das Gerät ohne USB-Verbindung
  unsigned long serialStart = millis();
  while (!Serial && (millis() - serialStart) < 2000UL) { ; }

  #ifdef WATCHDOG_ENABLED
    Serial.println(F("Watchdog: DEAKTIVIERT (Debug-Modus)"));
  #else
    Serial.println(F("Watchdog: nicht verfügbar (UNO R4)"));
  #endif

  motorInit();
  wifiInit();    // Heimnetz oder Einrichtungs-AP
  ntpTrySync();  // Zeit holen (nur wenn Heimnetz verbunden)
  webInit();

  // Manuelle Auslösung vorbereiten
  pinMode(MANUAL_TRIGGER_PIN, INPUT_PULLUP);
  pinMode(MANUAL_GROUND_PIN, OUTPUT);
  digitalWrite(MANUAL_GROUND_PIN, LOW);

  printStatusStartup();
  lastStatusMs = millis();
  bootMs = lastStatusMs;

  // Tageswechsel-Erkennung initialisieren
  if (gNtpSynced) {
    lastKnownDay = gNtpDay;
  }

  // Konfiguration aus RTC-RAM laden (oder Defaults)
  cfgLoadFromRtcRam();
  cfgApplyToRuntime();
}

void loop() {
  #ifdef WATCHDOG_ENABLED
    wdt_reset();
  #endif

  // DNS-Anfragen bearbeiten (Captive Portal im Einrichtungs-AP-Modus)
  dnsHandleRequests();

  // WLAN-Status prüfen und ggf. reconnecten
  if (WIFI_CHECK_INTERVAL_MS > 0) {
    unsigned long nowMs = millis();
    if (nowMs - lastWifiCheckMs >= WIFI_CHECK_INTERVAL_MS) {
      wifiCheckAndReconnect();
      lastWifiCheckMs = nowMs;
    }
  }

  // NTP periodisch neu synchronisieren
  if (wifiIsConnected() && !wifiIsSetupMode()) {
    if (!gNtpSynced || (millis() - gNtpLastSyncMs >= NTP_SYNC_INTERVAL_MS)) {
      ntpTrySync();
      gNtpLastSyncMs = millis();
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
    if (!manualConsumed && (now - manualLowSinceMs) >= 150UL && now >= manualLockoutUntilMs && !feedingInProgress) {
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
  int nowH_t = -1, nowM_t = -1, nowS_t = -1;
  ntpGetTime(nowH_t, nowM_t, nowS_t);
  if (gNtpSynced && nowH_t >= 0) {
    int nowSec = nowH_t * 3600 + nowM_t * 60 + nowS_t;
    if (lastNowSec < 0) lastNowSec = nowSec; // Initialisierung beim ersten Durchlauf

    // Tageswechsel erkennen → Marker zurücksetzen
    uint8_t todayId = (uint8_t)((millis() / 86400000UL) % 256);
    if (lastKnownDay != gNtpDay) {
      fedToday1 = false;
      fedToday2 = false;
      pendingSched1 = false;
      pendingSched2 = false;
      // Hinweis: lastNowSec NICHT zurücksetzen – Crossing-Logik ist wrap-aware
    }
    lastKnownDay = gNtpDay;

    // Zielzeiten (Sekunden und Minuten)
    int nowMin = nowH_t * 60 + nowM_t;
    int t1 = ((int)gCfg.h1) * 60 + (int)gCfg.m1;
    int t2 = ((int)gCfg.h2) * 60 + (int)gCfg.m2;
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
      bool trig1 = false;
      bool trig2 = false;

      // Termin 1: Crossing erkannt UND aktuell exakt :00 Sekunden -> auslösen oder defer
      if (!fedToday1 && crossed1 && (nowSec % 60) == 0) {
        if (deltaLastSec >= (int)MIN_GAP_MIN * 60) {
          // Log mit Sollzeit & aktueller RTC-Zeit
              Serial.print(F("TRIG:SCHED1@"));
          Serial.print(nowH_t); Serial.print(':');
          if (nowM_t < 10) Serial.print('0'); Serial.print(nowM_t); Serial.print(':');
          if (nowS_t < 10) Serial.print('0'); Serial.println(nowS_t);
          nextFeedSrc = SRC_SCHED1;
          requestImmediateFeed();
          fedToday1 = true;
          pendingSched1 = false;
          didTrigger = true;
          trig1 = true;
        } else {
          pendingSched1 = true;
          Serial.print(F("DEF:SCHED1(gap)@"));
          Serial.print(nowH_t); Serial.print(':');
          if (nowM_t < 10) Serial.print('0'); Serial.print(nowM_t); Serial.print(':');
          if (nowS_t < 10) Serial.print('0'); Serial.println(nowS_t);
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
            requestImmediateFeed();
            fedToday2 = true;
            pendingSched2 = false;
            didTrigger = true;
            trig2 = true;
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
          requestImmediateFeed();
          fedToday1 = true;
          pendingSched1 = false;
          didTrigger = true;
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
            requestImmediateFeed();
            fedToday2 = true;
            pendingSched2 = false;
          }
        }
      }
      
      // Direkter Fallback: Wenn Crossing vor >5s war und noch nicht gefüttert, sofort triggern
      if (!didTrigger && !fedToday1 && crossed1 && deltaLastSec >= (int)MIN_GAP_MIN * 60) {
        int secsSinceCrossing = nowSec - t1Sec; if (secsSinceCrossing < 0) secsSinceCrossing += 24 * 3600;
        if (secsSinceCrossing > 0 && secsSinceCrossing <= 10) {
          Serial.println(F("TRIG:SCHED1(late)"));
          nextFeedSrc = SRC_SCHED1;
          requestImmediateFeed();
          fedToday1 = true;
          pendingSched1 = false;
          didTrigger = true;
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
            requestImmediateFeed();
            fedToday2 = true;
            pendingSched2 = false;
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
  }
}

// ============================================================================
// Zentrale Sofortfütterung – aktualisiert Status und sperrt Reentrancy
// ============================================================================
void requestImmediateFeed() {
  if (feedingInProgress) return;
  feedingInProgress = true;

  // Motorfahrt ausführen (blocking Platzhalter)
  motorFeed(gCfg.steps, MOTOR_DIR_CW);

  // Last Feed Zeit aus NTP übernehmen (falls synchronisiert)
  {
    int fH = -1, fM = -1, fS = -1;
    ntpGetTime(fH, fM, fS);
    if (fH >= 0) {
      lastFeedHour   = fH;
      lastFeedMinute = fM;
      lastFeedSecond = fS;
      lastKnownDay   = gNtpDay;
    }
  }
  // Letzte Schritte merken
  lastFeedSteps = gCfg.steps;

  // Quelle der letzten Fütterung merken und zurücksetzen
  lastFeedSrc = nextFeedSrc;
  nextFeedSrc = SRC_NONE;

  // Optional: Stepper-Reset-Hook
  motorReset();

  feedingInProgress = false;
}
