// homepage.ino – Minimaler Webserver & Routen
#include <Arduino.h>
#include <WiFiS3.h>

extern WiFiServer server; // aus ap.ino
extern const int MOTOR_SCHRITTE; // aus main.ino (legacy)
extern const int FEED_STEPS_PER_SEC; // feste Schrittfrequenz für zeitbasierte Fütterung
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
// Neue API: RTC auf vom Client übergebene Gerätezeit setzen
void rtcSet(uint16_t y, uint8_t m, uint8_t d, uint8_t H, uint8_t M, uint8_t S);
// Motorsteuerung aus motor.ino (für Jog)
bool motorFeed(int steps, bool dirCW);
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
  client.print(F("HTTP/1.1 200 OK\r\n"));
  client.print(F("Content-Type: text/html; charset=utf-8\r\n"));
  client.print(F("Connection: close\r\n"));
  client.print(F("Cache-Control: no-store, no-cache, must-revalidate, max-age=0\r\n"));
  client.print(F("Pragma: no-cache\r\n"));
  client.print(F("Expires: 0\r\n"));
  // Basale Sicherheitsheader (schaden nicht, sparen RAM)
  client.print(F("X-Content-Type-Options: nosniff\r\n"));
  client.print(F("X-Frame-Options: DENY\r\n"));
  client.print(F("Referrer-Policy: no-referrer\r\n\r\n"));
  // HTML-Beginn
  client.print(F("<!DOCTYPE html>\n<html lang=\"de\">\n<head><meta charset=\"utf-8\">"));
  client.print(F("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"));
  client.print(F("<title>K.O.R.N. der Katastrophal Organisierte Runde Nahrungsmittelspender</title>"));
  client.print(F("<style>body{font-family:sans-serif;margin:16px}label{display:block;margin:8px 0}input[type=number]{width:5em}button{padding:8px 12px;margin-top:8px}hr{border:0;border-top:1px solid #000;margin:16px 0}</style>"));
  client.print(F("</head><body><h1>K.O.R.N. der Katastrophal Organisierte Runde Nahrungsmittelspender</h1>"));
}

static void sendFooter(WiFiClient &client) {
  client.print(F("<hr><small>"));
  client.print(F("K.O.R.N. Rev3 • Lizenz: Open-Source (siehe README) • GitHub: "));
  client.print(F("<a href=\"https://github.com/jinxblackzoo/K.O.R.N.\" target=\"_blank\" rel=\"noopener\">"));
  client.print(F("jinxblackzoo/K.O.R.N."));
  client.print(F("</a>"));
  client.print(F(" • Build: "));
  client.print(F(__DATE__));
  client.print(F(" "));
  client.print(F(__TIME__));
  client.print(F("</small></body></html>"));
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
  // Zeile: Zeit jetzt (self-updating via JS)
  client.print(F("<p><b>KORN-Zeit:</b> "));
  client.print(F("<span id=\"clock\""));
  client.print(F(" data-h=\"")); client.print(rtcOk ? nowH : -1); client.print(F("\""));
  client.print(F(" data-m=\"")); client.print(rtcOk ? nowM : -1); client.print(F("\""));
  client.print(F(" data-n1h=\"")); client.print((n1H>=0)?n1H:-1); client.print(F("\""));
  client.print(F(" data-n1m=\"")); client.print((n1M>=0)?n1M:-1); client.print(F("\""));
  client.print(F(" data-n2h=\"")); client.print((a2 && n2H>=0)?n2H:-1); client.print(F("\""));
  client.print(F(" data-n2m=\"")); client.print((a2 && n2M>=0)?n2M:-1); client.print(F("\""));
  client.print(F(">"));
  if (rtcOk) { snprintf(buf, sizeof(buf), "%02d:%02d", nowH, nowM); client.print(buf); }
  else client.print(F("--:--"));
  client.print(F("</span>"));
  // Zeile: Letzte Fütterung
  client.print(F(" | <b>Last:</b> "));
  if (lastH>=0 && lastM>=0) { snprintf(buf, sizeof(buf), "%02d:%02d", lastH, lastM); client.print(buf); }
  else client.print(F("--:--"));
  // Zeile: Next1 + Countdown
  client.print(F(" | <b>Next1:</b> "));
  if (n1H>=0 && n1M>=0) { snprintf(buf, sizeof(buf), "%02d:%02d", n1H, n1M); client.print(buf); }
  else client.print(F("--:--"));
  client.print(F(" (T-<span id=\"c1\">"));
  if (c1H>=0 && c1M>=0) { snprintf(buf, sizeof(buf), "%02d:%02d", c1H, c1M); client.print(buf); }
  else client.print(F("--:--"));
  client.print(F("</span>)"));
  // Zeile: Next2 + Countdown bzw. off
  client.print(F(" | <b>Next2:</b> "));
  if (a2 && n2H>=0 && n2M>=0) { snprintf(buf, sizeof(buf), "%02d:%02d", n2H, n2M); client.print(buf); }
  else client.print(F("--"));
  client.print(F(" [T-<span id=\"c2\">"));
  if (a2 && c2H>=0 && c2M>=0) { snprintf(buf, sizeof(buf), "%02d:%02d", c2H, c2M); client.print(buf); }
  else client.print(F("--:--"));
  client.print(F("</span>"));
  client.print(a2 ? F("") : F("off"));
  client.print(F("]"));
  // Zeile: Steps und Status
  client.print(F(" | <b>Steps:</b> ")); client.print(steps);
  client.print(F(" | <b>RTC:</b> ")); client.print(rtcOk?F("OK"):F("--"));
  client.print(F("</p>"));

  // AP-Infos (SSID/IP)
  client.print(F("<p><b>WLAN-ID:</b>  "));
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

  // Sekundenanzeige: aus Steps zurückrechnen
  int secs = cfgGetSteps() / (FEED_STEPS_PER_SEC > 0 ? FEED_STEPS_PER_SEC : 1000);
  if (secs < 1) secs = 1;
  client.print(F("<label>Motor-Laufzeit (Sekunden): <input type=\"number\" name=\"sec\" min=\"1\" max=\"60\" value=\""));
  client.print(secs);
  client.print(F("\"></label>"));


  client.print(F("<button type=\"submit\">Speichern</button></form>"));
  client.print(F("<hr><form method=\"POST\" action=\"/feed\"><button>Sofort füttern</button></form>"));
  // Zusätzlicher Trenner zwischen Sofort füttern und Blockadelöser
  client.print(F("<hr>"));
  // Blockadelöser: separater Rechtslauf mit Warnung, ohne Speicherung
  client.print(F("<div style=\"margin-top:8px\">"));
  client.print(F("<div style=\"padding:8px;border:1px solid #a00;background:#fee;color:#a00;font-weight:bold;border-radius:4px\">"));
  client.print(F("Achtung: Rechtslauf nur zum kurzfristigen Lösen von Blockaden verwenden. Nicht dauerhaft rückwärts drehen lassen!"));
  client.print(F("</div>"));
  client.print(F("<form id=\"unclog\" method=\"POST\" action=\"/jogcw\" style=\"margin-top:6px\" data-sps=\""));
  client.print(FEED_STEPS_PER_SEC);
  client.print(F("\">"));
  client.print(F("<label>Laufzeit (Sekunden): <input type=\"number\" name=\"sec2\" min=\"1\" max=\"60\" value=\""));
  client.print(2);
  client.print(F("\" style=\"width:6em\"></label>"));
  client.print(F("<input type=\"hidden\" name=\"n\" value=\"200\">"));
  client.print(F("<button id=\"unclogbtn\" type=\"button\" style=\"margin-left:12px\" onclick=\"(function(){var f=document.getElementById('unclog');var b=document.getElementById('unclogbtn');var sps=parseInt(f.getAttribute('data-sps'))||1000;var sec=parseInt(f.sec2.value)||1; if(sec<1)sec=1; if(sec>60)sec=60; var steps=sec*sps; if(steps<10)steps=10; if(steps>20000)steps=20000; f.n.value=steps; if(!confirm('Warnung: Rechtslauf nur zum Lösen von Blockaden. Fortfahren?'))return; b.disabled=true;setTimeout(function(){b.disabled=false;},1500);f.submit();})()\">Blockade lösen (Rechtslauf)</button>"));
  client.print(F("</form>"));
  client.print(F("</div>"));
  // Trenner vor Zeit-Update
  client.print(F("<hr>"));
  // Formular für RTC-Set mit Hidden-Feldern; JS füllt aktuelle Gerätezeit ein und sendet
  client.print(F(
    "<form id=\"rtcform\" method=\"POST\" action=\"/rtcset\">"
    "<input type=\"hidden\" name=\"y\"><input type=\"hidden\" name=\"m\">"
    "<input type=\"hidden\" name=\"d\"><input type=\"hidden\" name=\"H\">"
    "<input type=\"hidden\" name=\"M\"><input type=\"hidden\" name=\"S\">"
    "<button type=\"button\" onclick=\"(function(){var t=new Date();var f=document.getElementById('rtcform');"
    "f.y.value=t.getFullYear();f.m.value=(t.getMonth()+1);f.d.value=t.getDate();"
    "f.H.value=t.getHours();f.M.value=t.getMinutes();f.S.value=t.getSeconds();f.submit();})()\">KORN-Zeit Update</button>"
    "</form>"
  ));
  // Minimal-Skript: aktualisiert NUR die Status-Uhr (#clock) und Countdowns sekündlich
  client.print(F("<script>"));
  client.print(F("(function(){function pad(n){return (n<10?'0':'')+n;}function fmtHMS(s){var H=Math.floor(s/3600),R=s%3600,M=Math.floor(R/60),S=R%60;return H+':'+pad(M)+':'+pad(S);}function init(){var el=document.getElementById('clock');if(!el)return;var h=parseInt(el.getAttribute('data-h'));var m=parseInt(el.getAttribute('data-m'));if(isNaN(h)||isNaN(m)||h<0||m<0){var t=new Date();h=t.getHours();m=t.getMinutes();}var s=(new Date()).getSeconds();function tick(){s++;if(s>=60){s=0;m++;if(m>=60){m=0;h=(h+1)%24;}}el.textContent=pad(h)+':'+pad(m)+':'+pad(s);var nowSec=h*3600+m*60+s;var n1h=parseInt(el.getAttribute('data-n1h'));var n1m=parseInt(el.getAttribute('data-n1m'));if(!isNaN(n1h)&&!isNaN(n1m)&&n1h>=0&&n1m>=0){var diff=n1h*3600+n1m*60-nowSec;if(diff<0) diff+=86400;var c1=document.getElementById('c1');if(c1) c1.textContent=fmtHMS(diff);}var n2h=parseInt(el.getAttribute('data-n2h'));var n2m=parseInt(el.getAttribute('data-n2m'));if(!isNaN(n2h)&&!isNaN(n2m)&&n2h>=0&&n2m>=0){var diff2=n2h*3600+n2m*60-nowSec;if(diff2<0) diff2+=86400;var c2=document.getElementById('c2');if(c2) c2.textContent=fmtHMS(diff2);} } setInterval(tick,1000); tick();} if(document.readyState==='loading'){document.addEventListener('DOMContentLoaded',init);}else{init();}})();"));
  client.print(F("</script>"));
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
  String sh1, sm1, sh2, sm2, sa2, ssec;
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
  // Sekunden -> Schritte umrechnen: steps = sec * FEED_STEPS_PER_SEC
  int secVal = steps / (FEED_STEPS_PER_SEC > 0 ? FEED_STEPS_PER_SEC : 1000);
  if (kvFind(body, "sec", ssec)) secVal = (int)toUInt16(ssec, secVal);
  if (secVal < 1) secVal = 1; if (secVal > 60) secVal = 60; // 1..60s (65535-Steps-Limit)
  steps = secVal * (FEED_STEPS_PER_SEC > 0 ? FEED_STEPS_PER_SEC : 1000);

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
  } else if (reqLine.startsWith("POST /rtcset")) {
    // Body lesen
    String body;
    if (contentLength < 0) contentLength = 0;
    body.reserve((unsigned)contentLength);
    unsigned long dl2 = millis() + 1500UL;
    while ((int)body.length() < contentLength && millis() < dl2) {
      while (client.available() && (int)body.length() < contentLength) {
        body += (char)client.read();
      }
      delay(1);
    }
    // Felder extrahieren und setzen
    String sy, sm, sd, sH, sM, sS;
    if (kvFind(body, "y", sy) && kvFind(body, "m", sm) && kvFind(body, "d", sd)
        && kvFind(body, "H", sH) && kvFind(body, "M", sM) && kvFind(body, "S", sS)) {
      uint16_t y = toUInt16(sy, 2025);
      uint8_t m = (uint8_t)toUInt16(sm, 1);
      uint8_t d = (uint8_t)toUInt16(sd, 1);
      uint8_t H = (uint8_t)toUInt16(sH, 0);
      uint8_t M = (uint8_t)toUInt16(sM, 0);
      uint8_t S = (uint8_t)toUInt16(sS, 0);
      rtcSet(y, m, d, H, M, S);
    }
    // Redirect zurück
    client.print(F("HTTP/1.1 303 See Other\r\n"));
    client.print(F("Location: /\r\nConnection: close\r\n\r\n"));
  } else if (reqLine.startsWith("POST /jogcw") || reqLine.startsWith("POST /jogccw")) {
    // Body lesen, optionales 'n' (Schritte) parsen, Default 200
    String body;
    if (contentLength < 0) contentLength = 0;
    body.reserve((unsigned)contentLength);
    unsigned long dl3 = millis() + 1500UL;
    while ((int)body.length() < contentLength && millis() < dl3) {
      while (client.available() && (int)body.length() < contentLength) {
        body += (char)client.read();
      }
      delay(1);
    }
    String sn;
    int steps = 200;
    if (kvFind(body, "n", sn)) {
      int v = (int)toUInt16(sn, 200);
      if (v > 0 && v <= 20000) steps = v; // einfache Begrenzung
    }
    bool dirCW = reqLine.startsWith("POST /jogcw");
    // Ausführen (blockierend, wie feed), nutzt Relais/Buzzer aus motorFeed
    motorFeed(steps, dirCW);
    // Redirect zurück
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
