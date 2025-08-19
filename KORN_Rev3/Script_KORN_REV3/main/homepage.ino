// homepage.ino – Minimaler Webserver & Routen
#include <Arduino.h>
#include <WiFiS3.h>

extern WiFiServer server; // aus ap.ino
extern const int MOTOR_SCHRITTE; // aus main.ino
extern const bool MOTOR_DIR_CW;  // aus main.ino
bool motorFeed(int steps, bool dirCW); // aus motor.ino
// Status-Snapshot aus main.ino
void getStatusSnapshot(bool &rtcOk, int &nowH, int &nowM,
                       int &lastH, int &lastM,
                       int &n1H, int &n1M, int &n2H, int &n2M,
                       int &c1H, int &c1M, int &c2H, int &c2M,
                       bool &a2, int &steps);
// Zentrale Sofortfütterung aus main (vereinheitlicht Last/Marker)
void requestImmediateFeed();
// RTC Sync-API aus main.ino
void rtcSyncToCompile();
// Konfig-API aus main.ino für Webformular
int cfgGetH1();
int cfgGetM1();
int cfgGetH2();
int cfgGetM2();
bool cfgGetActive2();
int cfgGetSteps();
void cfgUpdateAndSave(uint8_t h1, uint8_t m1, uint8_t h2, uint8_t m2, bool active2, uint16_t steps);

// RAM-schonendes Streaming der Header/Footer direkt aus Flash
static void sendHeader(WiFiClient &client) {
  client.print(F("HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\n\r\n"));
  client.print(F("<!DOCTYPE html><html lang=\"de\"><head><meta charset=\"utf-8\">"));
  client.print(F("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"));
  client.print(F("<title>K.O.R.N.</title>"));
  client.print(F("<style>body{font-family:sans-serif;margin:16px}label{display:block;margin:8px 0}input[type=number]{width:5em}button{padding:8px 12px;margin-top:8px}</style>"));
  client.print(F("</head><body><h1>K.O.R.N.</h1>"));
}

static void sendFooter(WiFiClient &client) {
  client.print(F("<hr><small>K.O.R.N. Rev3</small></body></html>"));
}

void webInit() {
  // NOP – Server wird in apInit() gestartet
}

static void handleRoot(WiFiClient &client) {
  // Einfache Status-/Konfig-Seite mit Formular (RAM-schonend: gestreamt, F()-Strings)
  sendHeader(client);
  client.print(F("<h2>Status</h2>"));
  // Statuswerte abrufen
  bool rtcOk=false, a2=false; int nowH=-1,nowM=-1,lastH=-1,lastM=-1,n1H=-1,n1M=-1,n2H=-1,n2M=-1,c1H=-1,c1M=-1,c2H=-1,c2M=-1,steps=0;
  getStatusSnapshot(rtcOk, nowH, nowM, lastH, lastM, n1H, n1M, n2H, n2M, c1H, c1M, c2H, c2M, a2, steps);
  char buf[16];
  // Zeile: Zeit jetzt
  client.print(F("<p><b>Zeit:</b> "));
  if (rtcOk) { snprintf(buf, sizeof(buf), "%02d:%02d", nowH, nowM); client.print(buf); }
  else client.print(F("--:--"));
  // Zeile: Letzte Fütterung
  client.print(F(" | <b>Last:</b> "));
  if (lastH>=0 && lastM>=0) { snprintf(buf, sizeof(buf), "%02d:%02d", lastH, lastM); client.print(buf); }
  else client.print(F("--:--"));
  // Zeile: Next1 + Countdown
  client.print(F(" | <b>Next1:</b> "));
  if (n1H>=0 && n1M>=0) { snprintf(buf, sizeof(buf), "%02d:%02d", n1H, n1M); client.print(buf); }
  else client.print(F("--:--"));
  client.print(F(" (T-"));
  if (c1H>=0 && c1M>=0) { snprintf(buf, sizeof(buf), "%02d:%02d", c1H, c1M); client.print(buf); }
  else client.print(F("--:--"));
  client.print(F(")"));
  // Zeile: Next2 + Countdown bzw. off
  client.print(F(" | <b>Next2:</b> "));
  if (a2 && n2H>=0 && n2M>=0) { snprintf(buf, sizeof(buf), "%02d:%02d", n2H, n2M); client.print(buf); }
  else client.print(F("--"));
  client.print(F(" ["));
  if (a2 && c2H>=0 && c2M>=0) { snprintf(buf, sizeof(buf), "T-%02d:%02d", c2H, c2M); client.print(buf); }
  else client.print(F("off"));
  client.print(F("]"));
  // Zeile: Steps und Status
  client.print(F(" | <b>Steps:</b> ")); client.print(steps);
  client.print(F(" | <b>RTC:</b> ")); client.print(rtcOk?F("OK"):F("--"));
  client.print(F("</p>"));

  // AP-Infos (SSID/IP)
  client.print(F("<p><b>AP:</b> SSID "));
  client.print(WiFi.SSID());
  client.print(F(" | IP "));
  IPAddress ip = WiFi.localIP();
  char ipbuf[24];
  snprintf(ipbuf, sizeof(ipbuf), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
  client.print(ipbuf);
  client.print(F("</p>"));

  client.print(F("<h2>Konfiguration</h2>"));
  client.print(F("<form method=\"POST\" action=\"/save\">"));

  client.print(F("<label>Fütterung 1: <input type=\"number\" name=\"h1\" min=\"0\" max=\"23\" value=\""));
  client.print(cfgGetH1());
  client.print(F("\"> : "));
  client.print(F("<input type=\"number\" name=\"m1\" min=\"0\" max=\"59\" value=\""));
  client.print(cfgGetM1());
  client.print(F("\"></label>"));

  client.print(F("<label>Fütterung 2: <input type=\"number\" name=\"h2\" min=\"0\" max=\"23\" value=\""));
  client.print(cfgGetH2());
  client.print(F("\"> : "));
  client.print(F("<input type=\"number\" name=\"m2\" min=\"0\" max=\"59\" value=\""));
  client.print(cfgGetM2());
  client.print(F("\"></label>"));

  client.print(F("<label><input type=\"checkbox\" name=\"a2\" "));
  if (cfgGetActive2()) client.print(F("checked"));
  client.print(F("> Zweite Zeit aktiv</label>"));

  client.print(F("<label>Motor-Schritte: <input type=\"number\" name=\"steps\" min=\"1\" max=\"200000\" value=\""));
  client.print(cfgGetSteps());
  client.print(F("\"></label>"));

  client.print(F("<button type=\"submit\">Speichern</button></form>"));
  client.print(F("<hr><form method=\"POST\" action=\"/feed\"><button>Sofort füttern</button></form>"));
  client.print(F("<form method=\"POST\" action=\"/rtcsync\"><button>RTC auf Upload-Zeit setzen</button></form>"));
  sendFooter(client);
}

static void handleFeed(WiFiClient &client) {
  // Sofortfütterung
  requestImmediateFeed();
  sendHeader(client);
  client.print(F("<p>Fütterung ausgelöst.</p>"));
  client.print(F("<a href=\"/\">Zurück</a>"));
  sendFooter(client);
}

// Sehr einfache URL-Form-Parser für application/x-www-form-urlencoded
static bool kvFind(const String &body, const String &key, String &out) {
  int p = body.indexOf(key + "=");
  if (p < 0) return false;
  p += key.length() + 1;
  int e = body.indexOf('&', p);
  if (e < 0) e = body.length();
  out = body.substring(p, e);
  out.replace("+", " "); // minimal
  out.replace("%3A", ":"); // minimal decode für :
  return true;
}

static uint16_t toUInt16(const String &s, uint16_t defv) {
  long v = s.toInt();
  if (v < 0) return defv;
  if (v > 65535) v = 65535;
  return (uint16_t)v;
}

static void handleSave(WiFiClient &client, const String &body) {
  // Felder auslesen (robustheit: Defaults beibehalten, wenn Feld fehlt)
  String sh1, sm1, sh2, sm2, sa2, ssteps;
  int h1 = cfgGetH1();
  int m1 = cfgGetM1();
  int h2 = cfgGetH2();
  int m2 = cfgGetM2();
  bool a2 = cfgGetActive2();
  int steps = cfgGetSteps();

  if (kvFind(body, "h1", sh1)) h1 = sh1.toInt();
  if (kvFind(body, "m1", sm1)) m1 = sm1.toInt();
  if (kvFind(body, "h2", sh2)) h2 = sh2.toInt();
  if (kvFind(body, "m2", sm2)) m2 = sm2.toInt();
  a2 = body.indexOf("a2=") >= 0; // Checkbox gesetzt → enthalten
  if (kvFind(body, "steps", ssteps)) steps = (int)toUInt16(ssteps, steps);

  // In Konfiguration übernehmen und speichern
  cfgUpdateAndSave((uint8_t)h1, (uint8_t)m1, (uint8_t)h2, (uint8_t)m2, a2, (uint16_t)steps);

  // 303 Redirect zurück auf Startseite (verhindert doppeltes Absenden)
  client.print(F("HTTP/1.1 303 See Other\r\n"));
  client.print(F("Location: /\r\nConnection: close\r\n\r\n"));
}

void webHandleClient() {
  WiFiClient client = server.available();
  if (!client) return;

  // Request-Zeile
  String reqLine = client.readStringUntil('\n');
  reqLine.trim();
  // Header lesen: Content-Length extrahieren, bis Leerzeile
  int contentLength = -1;
  unsigned long t0 = millis();
  while (client.connected()) {
    String h = client.readStringUntil('\n');
    if (h.length() == 0) { if (millis()-t0 > 1500UL) break; else continue; }
    h.trim();
    if (h.length() == 0) break; // leere Zeile = Ende Header
    if (h.startsWith("Content-Length:")) {
      String v = h.substring(15);
      v.trim();
      contentLength = v.toInt();
    }
  }

  if (reqLine.startsWith("GET / ")) {
    handleRoot(client);
  } else if (reqLine.startsWith("POST /feed")) {
    handleFeed(client);
  } else if (reqLine.startsWith("POST /rtcsync")) {
    // RTC auf Kompilierzeit setzen und redirect zurück
    rtcSyncToCompile();
    client.print(F("HTTP/1.1 303 See Other\r\n"));
    client.print(F("Location: /\r\nConnection: close\r\n\r\n"));
  } else if (reqLine.startsWith("POST /save")) {
    // Body lesen (robust: exakt Content-Length, mit Timeout)
    String body;
    if (contentLength < 0) contentLength = 0; // falls Header fehlte
    body.reserve((unsigned)contentLength);
    unsigned long dl = millis() + 1500UL;
    while ((int)body.length() < contentLength && millis() < dl) {
      while (client.available() && (int)body.length() < contentLength) {
        body += (char)client.read();
      }
      delay(1);
    }
    handleSave(client, body);
  } else {
    // Fallback
    handleRoot(client);
  }

  delay(1);
  client.stop();
}
