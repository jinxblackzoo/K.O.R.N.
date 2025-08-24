/*
 * K.O.R.N. Rev3 – Modularer Sketch
 * Dateien:
 *  - main.ino      (Setup/Loop, Orchestrierung, Timer/Status)
 *  - motor.ino     (Motorsteuerung/Relais/ENA)
 *  - ap.ino        (Access Point / Netzwerk)
 *  - homepage.ino  (Webserver & Routen)
 *
 * Hardware (README):
 *  - DS1302: CLK=D7, DAT=D9, RST=D12
 *  - Motor:  PUL=D2, DIR=D3, ENA=D5, Relais=D8
 *  - Buzzer: D6 aktiv (HIGH=Ton)
 *  - Manuell: D10 (INPUT_PULLUP), D11 (OUTPUT LOW) -> Kurzschluss löst aus
 */

#include <Arduino.h>
#include <AccelStepper.h>
#include <WiFiS3.h>
#include <ThreeWire.h>      // Makuna: 3-Draht-Bus für DS1302
#include <RtcDS1302.h>      // Makuna: RTC-Klasse für DS1302

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

// DS1302 Pins (Makuna-Reihenfolge für ThreeWire: DIO, SCLK, CE)
#define DS1302_CLK 7
#define DS1302_DAT 9
#define DS1302_RST 12
static ThreeWire rtcBus(DS1302_DAT, DS1302_CLK, DS1302_RST);
static RtcDS1302<ThreeWire> Rtc(rtcBus);

// Manuelle Auslösung (Kurzschluss 10↔11)
#define MANUAL_TRIGGER_PIN 10  // INPUT_PULLUP
#define MANUAL_GROUND_PIN 11   // OUTPUT LOW

// Vorwärtsdeklarationen der Modul-APIs
void motorInit();
void motorEnable(bool on);
void motorReset();
bool motorFeed(int steps, bool dirCW);

void apInit(const char* ssid, const char* pass);
void webInit();
void webHandleClient();
// RTC-Sync auf Kompilierzeit
void rtcSyncToCompile();

// Kompakter Status-Snapshot für die Web-UI (RAM-schonend, keine Strings)
void getStatusSnapshot(bool &rtcOk, int &nowH, int &nowM,
                       int &lastH, int &lastM,
                       int &n1H, int &n1M, int &n2H, int &n2M,
                       int &c1H, int &c1M, int &c2H, int &c2M,
                       bool &a2, int &steps);

// Sofortfütterung zentral aus main anstoßen (für Web/Hardware)
void requestImmediateFeed();

// Konfig-Getter/Setter für Web-UI (entkoppelt von struct in anderen Modulen)
int cfgGetH1();
int cfgGetM1();
int cfgGetH2();
int cfgGetM2();
bool cfgGetActive2();
int cfgGetSteps();
void cfgUpdateAndSave(uint8_t h1, uint8_t m1, uint8_t h2, uint8_t m2, bool active2, uint16_t steps);
// Batterie-Indikator (softwarebasiert): RTC-Zeit gültig und Config aus RTC-RAM
bool batteryLikelyOK();

// ============================================================================
// Serielle Minimal-Ausgabe
// ============================================================================
static unsigned long lastStatusMs = 0;
static int lastFeedHour = -1;     // letzte Fütterung (HH)
static int lastFeedMinute = -1;   // letzte Fütterung (MM)
static int lastFeedSteps = -1;    // letzte Fütterung (Schritte)
static uint8_t lastKnownDay = 0;  // zur Erkennung des Tageswechsels
static bool fedToday1 = false;    // Marker: Fütterung 1 heute ausgeführt
static bool fedToday2 = false;    // Marker: Fütterung 2 heute ausgeführt
static bool feedingInProgress = false; // einfache Reentrancy-Sperre

// =========================================================================
// Konfiguration (Zeiten, Aktiv-Flag, Steps) – Ziel: DS1302-RAM (Persistenz)
// =========================================================================
struct KConfig {
  uint16_t magic;       // 0xBEEF als Marker
  uint8_t v;            // Versionsbyte
  uint8_t h1, m1;       // Zeit 1
  uint8_t h2, m2;       // Zeit 2
  uint8_t active2;      // 0/1 zweite Zeit aktiv
  uint16_t steps;       // Motor-Schritte
  uint16_t crc;         // einfache Prüfsumme
};

static KConfig gCfg;         // aktuelle Konfiguration im RAM
static bool gCfgValid = false; // Anzeige für Statuszeile CFG:OK/--
static bool gCfgFromRam = false; // Herkunft: true=aus RTC-RAM, false=Defaults

bool batteryLikelyOK() {
  // Heuristik: Backup-Batterie ist wahrscheinlich ok, wenn
  // 1) RTC eine gültige Zeit meldet und
  // 2) die Konfiguration erfolgreich aus dem DS1302-RAM gelesen wurde
  return Rtc.IsDateTimeValid() && gCfgFromRam;
}

static uint16_t cfgChecksum(const KConfig &c) {
  // Sehr einfache Prüfsumme über die payload-Felder (ohne magic, v, crc)
  uint32_t s = 0;
  s += c.h1 + c.m1 + c.h2 + c.m2 + c.active2;
  s += c.steps & 0xFF; s += (c.steps >> 8) & 0xFF;
  return (uint16_t)((s & 0xFFFFu) ^ 0xA5A5u);
}

// Auf Anforderung: RTC auf Kompilierzeit setzen (manueller Sync)
void rtcSyncToCompile() {
  RtcDateTime compiled(__DATE__, __TIME__);
  Rtc.SetDateTime(compiled);
}

// Auf Anforderung: RTC auf vom Client übergebene lokale Gerätezeit setzen
void rtcSet(uint16_t y, uint8_t m, uint8_t d, uint8_t H, uint8_t M, uint8_t S) {
  // einfache Plausibilitätsgrenzen (keine komplexe Monatsprüfung)
  if (m < 1 || m > 12) return;
  if (d < 1 || d > 31) return;
  if (H > 23 || M > 59 || S > 59) return;
  // Sicherstellen: Schreibschutz aus, Uhr läuft
  Rtc.SetIsWriteProtected(false);
  if (!Rtc.GetIsRunning()) {
    Rtc.SetIsRunning(true);
  }
  RtcDateTime t(y, m, d, H, M, S);
  Rtc.SetDateTime(t);
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

// Persistenz in DS1302-RAM – Makuna-API: wird separat verifiziert.
// Platzhalter, damit Web-Konfig bereits funktioniert (CFG-Status zeigt Gültigkeit an).
static void cfgLoadFromRtcRam() {
  // DS1302 besitzt 31 Bytes RAM – wir legen unsere 12-Byte-Struktur ab Offset 0 ab.
  // Makuna-API stellt GetMemory/SetMemory bereit.
  uint8_t buf[sizeof(KConfig)] = {0};
  bool ok = false;
  if (sizeof(KConfig) <= 31) {
    // Makuna-API: GetMemory(pValue, countBytes)
    if (Rtc.GetMemory(buf, (uint8_t)sizeof(buf))) {
      KConfig tmp;
      memcpy(&tmp, buf, sizeof(tmp));
      if (tmp.magic == 0xBEEF) {
        uint16_t c = cfgChecksum(tmp);
        if (c == tmp.crc) {
          gCfg = tmp;
          gCfgValid = true;
          gCfgFromRam = true;
          ok = true;
        }
      }
    }
  }
  if (!ok) {
    // Fallback auf Defaults
    cfgApplyDefaults();
    gCfgFromRam = false;
  }
}

static void cfgSaveToRtcRam() {
  // Struktur vorbereiten und in DS1302-RAM schreiben
  gCfg.crc = cfgChecksum(gCfg);
  uint8_t buf[sizeof(KConfig)] = {0};
  memcpy(buf, &gCfg, sizeof(gCfg));
  if (sizeof(KConfig) <= 31) {
    // Makuna-API: SetMemory(pValue, countBytes)
    Rtc.SetMemory(buf, (uint8_t)sizeof(buf));
  }
  gCfgValid = true;
  gCfgFromRam = true; // nach erfolgreichem Schreiben gilt Quelle=RAM
}

// Getter/Setter-Impl.
int cfgGetH1() { return gCfg.h1; }
int cfgGetM1() { return gCfg.m1; }
int cfgGetH2() { return gCfg.h2; }
int cfgGetM2() { return gCfg.m2; }
bool cfgGetActive2() { return gCfg.active2 != 0; }
int cfgGetSteps() { return gCfg.steps; }

void cfgUpdateAndSave(uint8_t h1, uint8_t m1, uint8_t h2, uint8_t m2, bool active2, uint16_t steps) {
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

static void printStatusStartup() {
  // RTC-Status und Zeit ausgeben (falls gültig), sonst Platzhalter
  char timeBuf[6] = "--:--";
  const char* rtcStat = "--";
  if (Rtc.IsDateTimeValid()) {
    RtcDateTime now = Rtc.GetDateTime();
    snprintf(timeBuf, sizeof(timeBuf), "%02u:%02u", now.Hour(), now.Minute());
    rtcStat = "OK";
  }
  const char* cfgStat = gCfgValid ? "OK" : "--";
  // IP dynamisch ermitteln
  IPAddress ip = WiFi.localIP();
  char ipBuf[24];
  snprintf(ipBuf, sizeof(ipBuf), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
  char line[180];
  snprintf(line, sizeof(line),
           "AP:KORN Pw:Chaosfeeder IP:%s | TIME:%s | Last:--:--(----) | Next1:--:--(--:--) Next2:-- [--] | Steps:%d | RTC:%s CFG:%s | Up:00:00",
           ipBuf, timeBuf, (int)gCfg.steps, rtcStat, (gCfgFromRam ? "RAM" : "DEF"));
  Serial.println(line);
}

// Hilfsfunktionen für Zeitformatierung und Next/Last
static void formatHM(char* buf, size_t len, int h, int m) {
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
void getStatusSnapshot(bool &rtcOk, int &nowH, int &nowM,
                       int &lastH, int &lastM,
                       int &n1H, int &n1M, int &n2H, int &n2M,
                       int &c1H, int &c1M, int &c2H, int &c2M,
                       bool &a2, int &steps) {
  rtcOk = Rtc.IsDateTimeValid();
  lastH = lastFeedHour; lastM = lastFeedMinute;
  a2 = (gCfg.active2 != 0);
  steps = gCfg.steps;
  nowH = nowM = -1; n1H = n1M = n2H = n2M = -1; c1H = c1M = c2H = c2M = -1;
  if (rtcOk) {
    RtcDateTime now = Rtc.GetDateTime();
    nowH = now.Hour(); nowM = now.Minute();
    computeNextTimes(nowH, nowM, n1H, n1M, n2H, n2M);
    diffToHM(nowH, nowM, n1H, n1M, c1H, c1M);
    diffToHM(nowH, nowM, n2H, n2M, c2H, c2M);
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
  if (secondIdx >= 0) { n2H = cH[secondIdx]; n2M = cM[secondIdx]; }
  else {
    // Falls firstIdx belegt war, zweite Zeit ist der früheste aktive des Folgetags (sofern verschieden)
    int best = -1;
    for (int i = 0; i < 2; ++i) if (cActive[i]) {
      if (best < 0 || cH[i] < cH[best] || (cH[i] == cH[best] && cM[i] < cM[best])) best = i;
    }
    if (best >= 0) { n2H = cH[best]; n2M = cM[best]; } else { n2H = n2M = -1; }
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
  const char* rtcStat = Rtc.IsDateTimeValid() ? "OK" : "--";
  // Countdowns vorab anlegen (außerhalb des if, damit später nutzbar)
  char c1Buf[6] = "--:--";
  char c2Buf[6] = "--:--";
  char n2Bracket[8] = "--"; // Inhalt in eckigen Klammern für Next2
  if (Rtc.IsDateTimeValid()) {
    RtcDateTime now = Rtc.GetDateTime();
    snprintf(timeBuf, sizeof(timeBuf), "%02u:%02u", now.Hour(), now.Minute());
    formatHM(lastBuf, sizeof(lastBuf), lastFeedHour, lastFeedMinute);
    int n1H=-1,n1M=-1,n2H=-1,n2M=-1;
    computeNextTimes(now.Hour(), now.Minute(), n1H, n1M, n2H, n2M);
    formatHM(n1Buf, sizeof(n1Buf), n1H, n1M);
    if (gCfg.active2) {
      formatHM(n2Buf, sizeof(n2Buf), n2H, n2M);
    } else {
      strncpy(n2Buf, "--", sizeof(n2Buf));
    }
    // Countdowns berechnen (Differenz bis zur Zielzeit, inkl. Tageswechsel)
    int c1H=-1, c1M=-1, c2H=-1, c2M=-1;
    diffToHM(now.Hour(), now.Minute(), n1H, n1M, c1H, c1M);
    diffToHM(now.Hour(), now.Minute(), n2H, n2M, c2H, c2M);
    if (c1H >= 0) snprintf(c1Buf, sizeof(c1Buf), "%02d:%02d", c1H, c1M);
    if (gCfg.active2 && c2H >= 0) snprintf(c2Buf, sizeof(c2Buf), "%02d:%02d", c2H, c2M);
    // Bracket-Text für Next2
    if (gCfg.active2) {
      snprintf(n2Bracket, sizeof(n2Bracket), "T-%s", c2Buf);
    } else {
      strncpy(n2Bracket, "off", sizeof(n2Bracket));
    }
  }
  const char* cfgStat = gCfgFromRam ? "RAM" : "DEF";
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
// RTC-Zeit bei Upload setzen (Platzhalter – konkrete DS1302 API folgt)
// ============================================================================
static void rtcInitAndMaybeSet() {
  // Initialisierung der DS1302 (Makuna)
  Rtc.Begin();
  // Schreibschutz aus
  Rtc.SetIsWriteProtected(false);
  // Falls Uhr nicht läuft: starten
  if (!Rtc.GetIsRunning()) {
    Rtc.SetIsRunning(true);
  }
  // Nur setzen, wenn Zeit ungültig (z.B. nach Batterieverlust)
  bool valid = Rtc.IsDateTimeValid();
  if (!valid) {
    // Fallback: Kompilierzeit (letzter Build)
    RtcDateTime compiled(__DATE__, __TIME__);
    Rtc.SetDateTime(compiled);
  }
}

// ============================================================================
// Setup / Loop
// ============================================================================
void setup() {
  Serial.begin(115200);
  while (!Serial) { ; }

  rtcInitAndMaybeSet();

  motorInit();         // Pins, Treiber in sicheren Zustand, ggf. AccelStepper init
  apInit("KORN", "Chaosfeeder"); // AP-Insellösung, feste IP folgt in ap.ino
  webInit();

  // Manuelle Auslösung vorbereiten
  pinMode(MANUAL_TRIGGER_PIN, INPUT_PULLUP);
  pinMode(MANUAL_GROUND_PIN, OUTPUT);
  digitalWrite(MANUAL_GROUND_PIN, LOW);

  printStatusStartup();
  lastStatusMs = millis();

  // Tageswechsel-Erkennung initialisieren
  if (Rtc.IsDateTimeValid()) {
    lastKnownDay = Rtc.GetDateTime().Day();
  }

  // Konfiguration aus RTC-RAM laden (oder Defaults)
  cfgLoadFromRtcRam();
  cfgApplyToRuntime();
}

void loop() {
  // Webrequests bedienen
  webHandleClient();

  // Periodische Minimal-Ausgabe
  const unsigned long intervalMs = (unsigned long)STATUS_INTERVAL_S * 1000UL;
  unsigned long now = millis();
  if (now - lastStatusMs >= intervalMs) {
    printStatusPeriodic();
    lastStatusMs = now;
  }

  // Hardware-Trigger: Kurzschluss 10↔11 löst Fütterung aus (einfach entprellt)
  static unsigned long lastTriggerMs = 0;
  static bool armed = true; // nur einmal pro Press
  int trig = digitalRead(MANUAL_TRIGGER_PIN); // HIGH=Ruhe, LOW=kurzgeschlossen
  if (trig == LOW && armed && (now - lastTriggerMs > 200UL)) {
    // Sofortfütterung zentral auslösen
    requestImmediateFeed();
    armed = false;
    lastTriggerMs = now;
  } else if (trig == HIGH && (now - lastTriggerMs > 300UL)) {
    armed = true;
  }

  // Geplante Fütterungen und Tagesreset
  if (Rtc.IsDateTimeValid()) {
    RtcDateTime nowRtc = Rtc.GetDateTime();

    // Tageswechsel erkennen → Marker zurücksetzen
    if (lastKnownDay != nowRtc.Day()) {
      fedToday1 = false;
      fedToday2 = false;
      lastKnownDay = nowRtc.Day();
    }

    // Nur auslösen, wenn nicht bereits in einer Fütterung
    if (!feedingInProgress) {
      // Fütterung 1 nach gCfg
      if (!fedToday1 && nowRtc.Hour() == gCfg.h1 && nowRtc.Minute() == gCfg.m1) {
        requestImmediateFeed();
        fedToday1 = true;
      }
      // Fütterung 2 (falls aktiv) nach gCfg
      if (gCfg.active2 && !fedToday2 && nowRtc.Hour() == gCfg.h2 && nowRtc.Minute() == gCfg.m2) {
        requestImmediateFeed();
        fedToday2 = true;
      }
    }
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

  // Last Feed Zeit aus RTC übernehmen (falls gültig)
  if (Rtc.IsDateTimeValid()) {
    RtcDateTime t = Rtc.GetDateTime();
    lastFeedHour = t.Hour();
    lastFeedMinute = t.Minute();
    lastKnownDay = t.Day(); // sicherstellen
  }
  // Letzte Schritte merken
  lastFeedSteps = gCfg.steps;

  // Optional: Stepper-Reset-Hook
  motorReset();

  feedingInProgress = false;
}
