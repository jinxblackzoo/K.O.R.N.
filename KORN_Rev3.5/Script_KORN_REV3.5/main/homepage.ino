// homepage.ino – Minimaler Webserver & Routen
#include <Arduino.h>
#include <WiFiS3.h>

extern WiFiServer server; // aus wifi_client.ino
extern const int MOTOR_SCHRITTE; // aus main.ino (legacy)
extern const int FEED_STEPS_PER_SEC; // feste Schrittfrequenz für zeitbasierte Fütterung
extern const bool MOTOR_DIR_CW;  // aus main.ino
// Motorsteuerung aus motor.ino
bool motorFeed(int steps, bool dirCW);
// Status-Snapshot aus main.ino (inkl. lastSrc)
void getStatusSnapshot(bool &ntpOk, int &nowH, int &nowM, int &nowS,
                       int &lastH, int &lastM,
                       int &n1H, int &n1M, int &n2H, int &n2M,
                       int &c1H, int &c1M, int &c2H, int &c2M,
                       bool &a2, int &steps, uint8_t &lastSrc, bool &delayWarning,
                       int &actualH, int &actualM, int &actualS);
// Zentrale Sofortfütterung aus main (vereinheitlicht Last/Marker)
void requestImmediateFeed();
// Asynchroner Web-Trigger (Flag in main.ino)
void requestFeedFromWebAsync();
// NTP-Status aus main.ino
bool ntpIsSynced();
// WLAN-Konfig speichern aus wifi_client.ino
void wifiCredSave(const char* ssid, const char* pass, const char* admin_pass);
bool wifiIsSetupMode();
bool wifiCredHasAdminPass();
bool wifiCredCheckAdminPass(const char* input);
// Konfig-API aus main.ino für Webformular
int cfgGetH1();
int cfgGetM1();
int cfgGetH2();
int cfgGetM2();
bool cfgGetActive2();
int cfgGetSteps();
void cfgUpdateAndSave(uint8_t h1, uint8_t m1, uint8_t h2, uint8_t m2, bool active2, uint32_t steps);
// Hilfsfunktion formatHM() aus main.ino
void formatHM(char* buf, size_t len, int h, int m);
// Event-Log aus main.ino
void logRenderHTML(WiFiClient &client);
void logEvent(const char* msg);

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
  client.print(F("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1, viewport-fit=cover\">"));
  client.print(F("<meta name=\"theme-color\" content=\"#1976d2\">"));
  client.print(F("<meta name=\"apple-mobile-web-app-capable\" content=\"yes\">"));
  client.print(F("<meta name=\"apple-mobile-web-app-status-bar-style\" content=\"black-translucent\">"));
  client.print(F("<meta name=\"apple-mobile-web-app-title\" content=\"K.O.R.N.\">"));
  client.print(F("<link rel=\"manifest\" href=\"/manifest.json\">"));
  client.print(F("<title>K.O.R.N. der Katastrophal Organisierte Runde Nahrungsmittelspender</title>"));
  client.print(F("<style>"));
  client.print(F("body{font-family:-apple-system,BlinkMacSystemFont,Segoe UI,Roboto,Helvetica,Arial,sans-serif;margin:16px;line-height:1.5}"));
  client.print(F("label{display:block;margin:12px 0;font-weight:500}"));
  client.print(F("input[type=number],input[type=tel]{width:6em;font-size:20px;padding:12px;border:2px solid #ccc;border-radius:6px;min-height:48px;box-sizing:border-box}"));
  client.print(F("input[type=checkbox]{width:24px;height:24px;margin-right:8px;vertical-align:middle}"));
  client.print(F("button{padding:16px 24px;margin-top:12px;font-size:18px;font-weight:600;border:none;border-radius:8px;min-height:56px;min-width:120px;background:#1976d2;color:#fff;cursor:pointer;-webkit-tap-highlight-color:transparent;touch-action:manipulation}"));
  client.print(F("button:active{background:#0d47a1;transform:scale(0.98)}"));
  client.print(F("button[type=submit]{background:#2e7d32}"));
  client.print(F("button[type=submit]:active{background:#1b5e20}"));
  client.print(F("button:disabled{background:#9e9e9e;cursor:not-allowed}"));
  client.print(F("hr{border:0;border-top:2px solid #ddd;margin:20px 0}"));
  client.print(F("h1{font-size:1.4rem;margin-bottom:8px}h2{font-size:1.2rem;margin-top:24px}"));
  client.print(F(".info-box{background:#e3f2fd;border:2px solid #1976d2;border-radius:8px;padding:16px;margin:16px 0;font-size:16px}"));
  client.print(F(".warning-box{background:#fff3e0;border:2px solid #f57c00;border-radius:8px;padding:12px;margin:12px 0}"));
  client.print(F("@media(min-width:600px){button{padding:14px 20px;font-size:16px;min-height:48px}}"));
  client.print(F("</style>"));
  client.print(F("</head><body><h1>K.O.R.N. der Katastrophal Organisierte Runde Nahrungsmittelspender</h1>"));
}

static void sendFooter(WiFiClient &client) {
  client.print(F("<hr><small>"));
  client.print(F("K.O.R.N. Rev3.5 • Lizenz: Open-Source (siehe README) • GitHub: "));
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
  // NOP – Server wird in wifiInit() gestartet
}

// Aktuelles Admin-Passwort (nur waehrend einer Request gueltig)
// Wird von webHandleClient() nach Auth-Check gesetzt, von handleRoot() zum
// Einbetten in Formulare und Links genutzt.
static String gCurrentAuthPass = "";
// Gecachte Versionen (pro Request einmal berechnet) – reduziert Heap-Allokationen
static String gCurrentAuthPassHtml = "";
static String gCurrentAuthPassUrl = "";

// HTML-Attribut-Escaping (verhindert XSS bei exotischen Zeichen im Passwort)
static String htmlEscape(const String &s) {
  String out;
  out.reserve(s.length() + 16);
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    switch (c) {
      case '&':  out += "&amp;"; break;
      case '<':  out += "&lt;"; break;
      case '>':  out += "&gt;"; break;
      case '"':  out += "&quot;"; break;
      case '\'': out += "&#39;"; break;
      default:   out += c; break;
    }
  }
  return out;
}

// URL-Encoding für Query-Strings (minimal: nur problematische Zeichen)
static String urlEncode(const String &s) {
  String out;
  out.reserve(s.length() * 2);
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
        (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
      out += c;
    } else {
      char hex[4];
      snprintf(hex, sizeof(hex), "%%%02X", (unsigned char)c);
      out += hex;
    }
  }
  return out;
}

// Sendet ein verstecktes ap-Feld in einem Formular, damit der Login persistiert
// Nutzt gecachte HTML-escape-Variante (pro Request einmal berechnet)
static void sendAuthHidden(WiFiClient &client) {
  if (gCurrentAuthPassHtml.length() == 0) return;
  client.print(F("<input type=\"hidden\" name=\"ap\" value=\""));
  client.print(gCurrentAuthPassHtml);
  client.print(F("\">"));
}

// Liefert "?ap=..." Suffix fuer Redirects/Links (oder leer) – nutzt gecachte URL-Variante
static String authQuerySuffix() {
  if (gCurrentAuthPassUrl.length() == 0) return "";
  return String("?ap=") + gCurrentAuthPassUrl;
}

// URL-Decode (für %XX Sequenzen aus Query-String)
static String urlDecode(const String &s) {
  String out;
  out.reserve(s.length());
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '+') {
      out += ' ';
    } else if (c == '%' && i + 2 < s.length()) {
      char hex[3] = { s[i+1], s[i+2], 0 };
      out += (char)strtol(hex, nullptr, 16);
      i += 2;
    } else {
      out += c;
    }
  }
  return out;
}

// Extrahiert Admin-Passwort aus Query-String (?ap=...) oder Body (URL-decodiert)
static String extractAdminPass(const String &reqLine, const String &body) {
  // Prüfe Query-String
  int q = reqLine.indexOf("?");
  if (q >= 0) {
    int ap = reqLine.indexOf("ap=", q);
    if (ap >= 0) {
      ap += 3;  // Länge von "ap="
      int end = reqLine.indexOf('&', ap);
      if (end < 0) end = reqLine.indexOf(' ', ap);
      if (end < 0) end = reqLine.length();
      return urlDecode(reqLine.substring(ap, end));
    }
  }
  // Prüfe Body (für POST-Requests)
  if (body.length() > 0) {
    String ap;
    if (kvFind(body, "ap", ap)) return urlDecode(ap);
  }
  return "";
}

// Zeigt Login-Seite wenn Admin-Passwort gesetzt und nicht korrekt übergeben
// Gibt true zurück wenn Zugriff erlaubt, false wenn Login-Seite gesendet wurde
static bool requireAuth(WiFiClient &client, const String &reqLine, const String &body) {
  if (!wifiCredHasAdminPass()) return true;  // kein Schutz aktiv
  String input = extractAdminPass(reqLine, body);
  if (wifiCredCheckAdminPass(input.c_str())) return true;  // Passwort korrekt oder leer
  // Login-Seite anzeigen
  client.print(F("HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nConnection: close\r\n\r\n"));
  client.print(F("<!DOCTYPE html><html lang=\"de\"><head><meta charset=\"utf-8\">"));
  client.print(F("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"));
  client.print(F("<title>K.O.R.N. – Login</title>"));
  client.print(F("<style>body{font-family:-apple-system,BlinkMacSystemFont,Segoe UI,Roboto,sans-serif;margin:32px 16px;text-align:center;background:#f5f5f5}"));
  client.print(F(".card{background:#fff;border-radius:12px;padding:32px 24px;box-shadow:0 2px 8px rgba(0,0,0,0.1);max-width:400px;margin:0 auto}"));
  client.print(F("input{width:100%;font-size:20px;margin:12px 0;padding:16px;border:2px solid #ddd;border-radius:8px;box-sizing:border-box}"));
  client.print(F("button{width:100%;padding:18px;font-size:18px;font-weight:600;border:none;border-radius:8px;background:#1976d2;color:#fff;cursor:pointer}"));
  client.print(F("h1{font-size:1.5rem;margin:12px 0}"));
  client.print(F(".error{color:#c62828;margin:12px 0}"));
  client.print(F("</style></head><body>"));
  client.print(F("<div class=\"card\">"));
  client.print(F("<h1>🔒 K.O.R.N. geschützt</h1>"));
  if (input.length() > 0) {
    client.print(F("<div class=\"error\">Falsches Passwort</div>"));
  }
  client.print(F("<form method=\"GET\" action=\"/\">"));
  client.print(F("<input type=\"password\" name=\"ap\" placeholder=\"Admin-Passwort\" required autofocus>"));
  client.print(F("<button type=\"submit\">Anmelden</button>"));
  client.print(F("</form></div></body></html>"));
  return false;
}

static void handleRoot(WiFiClient &client) {
  // Einfache Status-/Konfig-Seite mit Formular (RAM-schonend: gestreamt, F()-Strings)
  // Auto-Redirect zu /setup wenn im Einrichtungs-AP-Modus (nutzerfreundlich für Handy-Einrichtung)
  if (wifiIsSetupMode()) {
    // Setup-Seite direkt ausliefern (keine Weiterleitung) wenn im Einrichtungs-AP-Modus
    client.print(F("HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nConnection: close\r\n\r\n"));
    client.print(F("<!DOCTYPE html><html lang=\"de\"><head><meta charset=\"utf-8\">"));
    client.print(F("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1, viewport-fit=cover\">"));
    client.print(F("<meta name=\"theme-color\" content=\"#1976d2\">"));
    client.print(F("<meta name=\"apple-mobile-web-app-capable\" content=\"yes\">"));
    client.print(F("<meta name=\"apple-mobile-web-app-title\" content=\"K.O.R.N. Setup\">"));
    client.print(F("<title>K.O.R.N. WLAN-Einrichtung</title>"));
    client.print(F("<style>"));
    client.print(F("body{font-family:-apple-system,BlinkMacSystemFont,Segoe UI,Roboto,Helvetica,Arial,sans-serif;margin:16px;line-height:1.5;background:#f5f5f5}"));
    client.print(F(".card{background:#fff;border-radius:12px;padding:20px;margin:12px 0;box-shadow:0 2px 8px rgba(0,0,0,0.1)}"));
    client.print(F("input{width:100%;font-size:20px;margin:10px 0;padding:14px;border:2px solid #ddd;border-radius:8px;box-sizing:border-box;min-height:52px}"));
    client.print(F("input:focus{border-color:#1976d2;outline:none}"));
    client.print(F("button{width:100%;padding:18px;font-size:18px;font-weight:600;border:none;border-radius:8px;min-height:56px;background:#1976d2;color:#fff;cursor:pointer;margin-top:12px}"));
    client.print(F("button:active{background:#0d47a1;transform:scale(0.98)}"));
    client.print(F("h1{font-size:1.4rem;margin:0 0 12px 0}h2{font-size:1.1rem;margin:0 0 8px 0;color:#1976d2}"));
    client.print(F(".step{display:flex;gap:10px;margin:6px 0;font-size:15px;align-items:flex-start}"));
    client.print(F(".num{background:#1976d2;color:#fff;border-radius:50%;min-width:24px;height:24px;display:flex;align-items:center;justify-content:center;font-size:13px;font-weight:700}"));
    client.print(F(".warn{background:#fff8e1;border:1px solid #f9a825;border-radius:8px;padding:10px;font-size:13px;margin-top:8px}"));
    client.print(F("</style></head><body>"));
    // Formular-Card
    client.print(F("<div class=\"card\">"));
    client.print(F("<h1>🔧 K.O.R.N. WLAN-Einrichtung</h1>"));
    client.print(F("<form method=\"POST\" action=\"/wifisetup\">"));
    client.print(F("<label>WLAN-Name (SSID):<input type=\"text\" name=\"ssid\" autocomplete=\"off\" placeholder=\"MeinHeimnetz\" required></label>"));
    client.print(F("<label>Passwort:<input type=\"password\" name=\"pass\" autocomplete=\"off\" placeholder=\"WLAN-Passwort (min. 8 Zeichen)\" minlength=\"8\" maxlength=\"63\" required></label>"));
    client.print(F("<hr style=\"margin:16px 0;border-color:#ddd\">"));
    client.print(F("<label>🔒 Admin-Passwort (optional):<input type=\"password\" name=\"admin_pass\" autocomplete=\"off\" placeholder=\"Web-UI schützen (leer lassen = kein Schutz)\" minlength=\"4\" maxlength=\"32\"></label>"));
    client.print(F("<div style=\"font-size:13px;color:#666;margin:-8px 0 12px 0\">Falls gesetzt, wird bei jedem Zugriff auf KORN nach diesem Passwort gefragt.</div>"));
    client.print(F("<button type=\"submit\">💾 Speichern & Verbinden</button>"));
    client.print(F("</form>"));
    client.print(F("<div class=\"warn\">⚠️ <strong>Nur WPA2!</strong> Der Arduino unterstützt kein WPA3. Bei Verbindungsproblemen im Router auf WPA2 umstellen.</div>"));
    client.print(F("</div>"));
    // Was passiert nach dem Speichern?
    client.print(F("<div class=\"card\">"));
    client.print(F("<h2>📋 Was passiert nach dem Speichern?</h2>"));
    client.print(F("<div class=\"step\"><div class=\"num\">1</div><div>Arduino startet neu (~15 Sek.) und verbindet sich mit deinem Heimnetz</div></div>"));
    client.print(F("<div class=\"step\"><div class=\"num\">2</div><div>Verbinde dein Handy wieder mit deinem <strong>Heimnetz</strong></div></div>"));
    client.print(F("<div class=\"step\"><div class=\"num\">3</div><div>Öffne im Browser: <strong>http://korn</strong> (FritzBox, Speedport, OpenWRT)</div></div>"));
    client.print(F("<div class=\"step\"><div class=\"num\">4</div><div>Falls nicht erreichbar: IP aus Router-Oberfläche holen (Gerätename \"KORN\")</div></div>"));
    client.print(F("<div class=\"step\"><div class=\"num\">5</div><div>Adresse als <strong>Lesezeichen speichern!</strong></div></div>"));
    client.print(F("</div></body></html>"));
    return;
  }
  sendHeader(client);
  client.print(F("<h2>Status</h2>"));
  // Statuswerte abrufen
  bool ntpOk=false, a2=false, delayWarning=false; int nowH=-1,nowM=-1,nowS=-1,lastH=-1,lastM=-1,n1H=-1,n1M=-1,n2H=-1,n2M=-1,c1H=-1,c1M=-1,c2H=-1,c2M=-1,steps=0,actualH=-1,actualM=-1,actualS=-1; uint8_t lastSrc=0;
  getStatusSnapshot(ntpOk, nowH, nowM, nowS, lastH, lastM, n1H, n1M, n2H, n2M, c1H, c1M, c2H, c2M, a2, steps, lastSrc, delayWarning, actualH, actualM, actualS);
  char buf[16];
  // Zeile: Zeit jetzt (self-updating via JS)
  client.print(F("<p><b>KORN-Zeit:</b> "));
  client.print(F("<span id=\"clock\""));
  client.print(F(" data-h=\"")); client.print(ntpOk ? nowH : -1); client.print(F("\""));
  client.print(F(" data-m=\"")); client.print(ntpOk ? nowM : -1); client.print(F("\""));
  client.print(F(" data-s=\"")); client.print(ntpOk ? nowS : -1); client.print(F("\""));
  client.print(F(" data-n1h=\"")); client.print((n1H>=0)?n1H:-1); client.print(F("\""));
  client.print(F(" data-n1m=\"")); client.print((n1M>=0)?n1M:-1); client.print(F("\""));
  client.print(F(" data-n2h=\"")); client.print((a2 && n2H>=0)?n2H:-1); client.print(F("\""));
  client.print(F(" data-n2m=\"")); client.print((a2 && n2M>=0)?n2M:-1); client.print(F("\""));
  client.print(F(">"));
  if (ntpOk) { snprintf(buf, sizeof(buf), "%02d:%02d:%02d", nowH, nowM, nowS); client.print(buf); }
  else client.print(F("--:--:--"));
  client.print(F("</span></p>"));
  
  // Letzte Fütterung
  client.print(F("<p><b>Letzte Fütterung:</b> <span id=\"lastfeed\" data-h=\""));
  client.print((lastH>=0)?lastH:-1);
  client.print(F("\" data-m=\""));
  client.print((lastM>=0)?lastM:-1);
  client.print(F("\" data-src=\""));
  client.print(lastSrc);
  client.print(F("\">")); 
  if (lastH>=0 && lastM>=0) { snprintf(buf, sizeof(buf), "%02d:%02d", lastH, lastM); client.print(buf); }
  else client.print(F("--:--"));
  // Quelle der letzten Fütterung
  switch (lastSrc) {
    case 1: client.print(F(" [Manuell]")); break;
    case 2: client.print(F(" [Web]")); break;
    case 3: client.print(F(" [Fütterung 1]")); break;
    case 4: client.print(F(" [Fütterung 2]")); break;
    default: break;
  }
  client.print(F("</span></p>"));
  
  // Nächste Fütterung (zeitlich nächste)
  client.print(F("<p><b>Nächste Fütterung:</b> "));
  if (n1H>=0 && n1M>=0) { 
    snprintf(buf, sizeof(buf), "%02d:%02d", n1H, n1M); 
    client.print(buf); 
    // Zeit bis zur nächsten Fütterung direkt dahinter
    client.print(F(" (in <span id=\"c1\">"));
    if (c1H>=0 && c1M>=0) { snprintf(buf, sizeof(buf), "%02d:%02d", c1H, c1M); client.print(buf); }
    else client.print(F("--:--"));
    client.print(F("</span>)"));
    if (delayWarning) {
      client.print(F(" <span style=\"color:#d63384;font-size:0.9em\">⚠ Verzögert durch 2-Min-Mindestabstand"));
      if (actualH >= 0 && actualM >= 0 && actualS >= 0) {
        client.print(F(" → tatsächlich um "));
        if (actualH < 10) client.print('0'); client.print(actualH); client.print(':');
        if (actualM < 10) client.print('0'); client.print(actualM); client.print(':');
        if (actualS < 10) client.print('0'); client.print(actualS);
      }
      client.print(F("</span>"));
    }
  } else {
    client.print(F("--:--"));
  }
  client.print(F("</p>"));
  
  // Übernächste Fütterung
  client.print(F("<p><b>Übernächste Fütterung:</b> "));
  if (a2 && n2H>=0 && n2M>=0) { 
    formatHM(buf, sizeof(buf), n2H, n2M);
    client.print(buf);
    client.print(F(" (in <span id=\"c2\">"));
    formatHM(buf, sizeof(buf), c2H, c2M);
    client.print(buf);
    client.print(F("</span>)"));
  } else if (a2) {
    client.print(F("--:--"));
  } else {
    client.print(F("deaktiviert"));
  }
  client.print(F("</p>"));
  
  // System-Status
  int laufzeitSek = steps / 1000; // steps / FEED_STEPS_PER_SEC
  client.print(F("<p><b>Laufzeit:</b> ")); client.print(laufzeitSek); client.print(F("s"));
  client.print(F(" | <b>NTP-Status:</b> ")); client.print(ntpOk?F("OK"):F("Kein Sync"));
  client.print(F(" | <b>WLAN:</b> ")); client.print(wifiIsSetupMode()?F("Einrichtungs-AP aktiv"):F("Heimnetz"));
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
  client.print(F("<form method=\"POST\" action=\"/save\" autocomplete=\"off\" onsubmit=\"(function(f){var b=f.querySelector('button[type=submit]'); if(b){b.disabled=true;b.textContent='Speichere…';}})(this)\">"));
  sendAuthHidden(client);

  client.print(F("<label>Fütterung 1: <input type=\"number\" name=\"h1\" min=\"0\" max=\"23\" inputmode=\"numeric\" pattern=\"[0-9]*\" enterkeyhint=\"done\" value=\""));
  client.print(cfgGetH1());
  client.print(F("\"> : "));
  client.print(F("<input type=\"number\" name=\"m1\" min=\"0\" max=\"59\" inputmode=\"numeric\" pattern=\"[0-9]*\" enterkeyhint=\"done\" value=\""));
  client.print(cfgGetM1());
  client.print(F("\"></label>"));

  client.print(F("<label>Fütterung 2: <input type=\"number\" name=\"h2\" min=\"0\" max=\"23\" inputmode=\"numeric\" pattern=\"[0-9]*\" enterkeyhint=\"done\" value=\""));
  client.print(cfgGetH2());
  client.print(F("\"> : "));
  client.print(F("<input type=\"number\" name=\"m2\" min=\"0\" max=\"59\" inputmode=\"numeric\" pattern=\"[0-9]*\" enterkeyhint=\"done\" value=\""));
  client.print(cfgGetM2());
  client.print(F("\"></label>"));

  client.print(F("<label><input type=\"checkbox\" name=\"a2\" "));
  if (cfgGetActive2()) client.print(F("checked"));
  client.print(F("> Zweite Zeit aktiv</label>"));

  // Sekundenanzeige: aus Steps zurückrechnen
  int secs = cfgGetSteps() / (FEED_STEPS_PER_SEC > 0 ? FEED_STEPS_PER_SEC : 1000);
  if (secs < 1) secs = 1;
  client.print(F("<label>Motor-Laufzeit (Sekunden): <input type=\"number\" name=\"sec\" min=\"1\" max=\"600\" inputmode=\"numeric\" pattern=\"[0-9]*\" enterkeyhint=\"done\" value=\""));
  client.print(secs);
  client.print(F("\"></label>"));
  // Hinweise zu Limits und thermischer Belastung
  client.print(F("<p><small><strong>Hinweise:</strong><br>"));
  client.print(F("• Mindestabstand zwischen Fütterungen: 2 Minuten<br>"));
  client.print(F("• Max. Laufzeit: 10 Minuten (thermische Belastung NEMA17)<br>"));
  client.print(F("• Mittlere Futteraufnahme: 110-130g/Henne/Tag "));
  client.print(F("(Quelle: <a href=\"https://www.huehner-info.de/infos/futter_bestandteile3.htm\" target=\"_blank\" rel=\"noopener\">huehner-info.de</a>)"));
  client.print(F("</small></p>"));
  client.print(F("<button type=\"submit\">Speichern</button></form>"));
  client.print(F("<hr>"));
  client.print(F("<div class=\"info-box\">"));
  client.print(F("<strong>ℹ️ Hinweis:</strong> Während der Motor läuft, bleibt die Seite im Lademodus – das ist normal. Sobald der Motor stoppt, lädt sie wieder."));
  client.print(F("</div>"));
  client.print(F("<form method=\"POST\" action=\"/feed\" onsubmit=\"(function(f){var b=f.querySelector('button'); if(b){b.disabled=true;b.textContent='Wird ausgelöst…';}})(this)\">"));
  sendAuthHidden(client);
  client.print(F("<button>Sofort füttern</button></form>"));
  // Zusätzlicher Trenner zwischen Sofort füttern und Blockadelöser
  client.print(F("<hr>"));
  // Blockadelöser: separater Rechtslauf mit Warnung, ohne Speicherung
  client.print(F("<div style=\"margin-top:16px\">"));
  client.print(F("<div class=\"warning-box\">"));
  client.print(F("<strong>⚠️ Achtung:</strong> Rechtslauf nur zum kurzfristigen Lösen von Blockaden verwenden. Nicht dauerhaft rückwärts drehen lassen!"));
  client.print(F("</div>"));
  client.print(F("<form id=\"unclog\" method=\"POST\" action=\"/jogcw\" style=\"margin-top:6px\">"));
  sendAuthHidden(client);
  client.print(F("<label>Laufzeit (Sekunden): <input type=\"number\" name=\"sec2\" min=\"1\" max=\"60\" value=\"2\" style=\"width:6em\"></label>"));
  client.print(F("<button type=\"submit\" style=\"margin-left:12px\">Blockade lösen (Rechtslauf)</button>"));
  client.print(F("</form>"));
  client.print(F("</div>"));
  // Trenner vor Zeit-Update
  client.print(F("<hr>"));
  // NTP-Status Anzeige (deaktivierter Button)
  client.print(F("<div style=\"margin-top:6px\">"));
  if (ntpIsSynced()) {
    client.print(F("<button type=\"button\" disabled style=\"background:#2e7d32;color:#fff;border:none;padding:6px 10px;border-radius:4px;opacity:0.9;cursor:default\">NTP-Zeit synchronisiert</button>"));
  } else {
    client.print(F("<button type=\"button\" disabled style=\"background:#c62828;color:#fff;border:none;padding:6px 10px;border-radius:4px;opacity:0.95;cursor:default\">NTP nicht synchronisiert</button>"));
  }
  client.print(F("</div>"));
  // Diagnose-Log Link
  client.print(F("<div style=\"margin-top:12px\"><a href=\"/log"));
  client.print(authQuerySuffix());
  client.print(F("\"><button type=\"button\" style=\"background:#455a64;color:#fff;border:none;padding:8px 12px;border-radius:4px;font-size:14px;cursor:pointer\">📋 Event-Log anzeigen</button></a></div>"));
  // Factory Reset Button (unter NTP-Status)
  client.print(F("<div style=\"margin-top:12px\">"));
  client.print(F("<form method=\"POST\" action=\"/reset\" onsubmit=\"return confirm('Wirklich alle Einstellungen löschen? WLAN-Zugangsdaten und Konfiguration werden zurückgesetzt.');\">"));
  sendAuthHidden(client);
  client.print(F("<button type=\"submit\" style=\"background:#c62828;color:#fff;border:none;padding:8px 12px;border-radius:4px;font-size:14px;cursor:pointer\">🗑️ Auf Werkseinstellungen zurücksetzen</button>"));
  client.print(F("</form>"));
  client.print(F("</div>"));
  // Minimal-Skript: aktualisiert die Status-Uhr (#clock) und Countdowns (#c1, #c2) sekündlich, basierend auf RTC-H:M:S + Date.now()-Delta
  // c1 = Countdown zur nächsten Fütterung, c2 = Countdown zur übernächsten Fütterung
  client.print(F("<script>"));
  if (gCurrentAuthPassUrl.length() > 0) {
    // URL-encoded ablegen, damit JS-Sonderzeichen nicht ausbrechen koennen
    client.print(F("window.__ap=decodeURIComponent('"));
    client.print(gCurrentAuthPassUrl);
    client.print(F("');"));
  }
  client.print(F("(function(){function pad(n){return (n<10?'0':'')+n;}function fmtHMS(s){var H=Math.floor(s/3600),R=s%3600,M=Math.floor(R/60),S=R%60;return H+':'+pad(M)+':'+pad(S);}var lastUpdate=0;function updateStatus(){var xhr=new XMLHttpRequest();xhr.open('GET','/status'+(window.__ap?('?ap='+encodeURIComponent(window.__ap)):''),true);xhr.onreadystatechange=function(){if(xhr.readyState===4&&xhr.status===200){try{var data=JSON.parse(xhr.responseText);var lf=document.getElementById('lastfeed');if(lf&&data.lastH>=0&&data.lastM>=0){var src='';switch(data.lastSrc){case 1:src=' [Manuell]';break;case 2:src=' [Web]';break;case 3:src=' [Fütterung 1]';break;case 4:src=' [Fütterung 2]';break;}lf.textContent=pad(data.lastH)+':'+pad(data.lastM)+src;lf.setAttribute('data-h',data.lastH);lf.setAttribute('data-m',data.lastM);lf.setAttribute('data-src',data.lastSrc);}}catch(e){}}};xhr.send();}function init(){var el=document.getElementById('clock');if(!el)return;var h=parseInt(el.getAttribute('data-h'));var m=parseInt(el.getAttribute('data-m'));var s=parseInt(el.getAttribute('data-s'));var hasRtc=!(isNaN(h)||isNaN(m)||isNaN(s)||h<0||m<0||s<0);if(!hasRtc){var t=new Date();h=t.getHours();m=t.getMinutes();s=t.getSeconds();}var base=(h*3600+m*60+s)%86400;var t0=Date.now();function render(){var elapsed=Math.floor((Date.now()-t0)/1000);var nowSec=(base+elapsed)%86400;var H=Math.floor(nowSec/3600),R=nowSec%3600,M=Math.floor(R/60),S=R%60;el.textContent=pad(H)+':'+pad(M)+':'+pad(S);var n1h=parseInt(el.getAttribute('data-n1h')),n1m=parseInt(el.getAttribute('data-n1m'));if(!isNaN(n1h)&&!isNaN(n1m)&&n1h>=0&&n1m>=0){var diff=n1h*3600+n1m*60-nowSec;if(diff<0)diff+=86400;var c1=document.getElementById('c1');if(c1)c1.textContent=fmtHMS(diff);}var n2h=parseInt(el.getAttribute('data-n2h')),n2m=parseInt(el.getAttribute('data-n2m'));if(!isNaN(n2h)&&!isNaN(n2m)&&n2h>=0&&n2m>=0){var diff2=n2h*3600+n2m*60-nowSec;if(diff2<0)diff2+=86400;var c2=document.getElementById('c2');if(c2)c2.textContent=fmtHMS(diff2);}if(elapsed%10===0&&elapsed!==lastUpdate){updateStatus();lastUpdate=elapsed;}}setInterval(render,1000);render();}if(document.readyState==='loading'){document.addEventListener('DOMContentLoaded',init);}else{init();}})();"));
  client.print(F("</script>"));
  sendFooter(client);
}

static void handleStatus(WiFiClient &client) {
  // JSON-Status für AJAX-Updates
  bool ntpOk, a2, delayWarning;
  int nowH, nowM, nowS, lastH, lastM, n1H, n1M, n2H, n2M, c1H, c1M, c2H, c2M, steps, actualH, actualM, actualS;
  uint8_t lastSrc;
  getStatusSnapshot(ntpOk, nowH, nowM, nowS, lastH, lastM, n1H, n1M, n2H, n2M, c1H, c1M, c2H, c2M, a2, steps, lastSrc, delayWarning, actualH, actualM, actualS);
  
  client.print(F("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nConnection: close\r\n\r\n"));
  client.print(F("{"));
  client.print(F("\"lastH\":"));
  client.print((lastH>=0)?lastH:-1);
  client.print(F(",\"lastM\":"));
  client.print((lastM>=0)?lastM:-1);
  client.print(F(",\"lastSrc\":"));
  client.print(lastSrc);
  client.print(F("}"));
}

static void handleFeed(WiFiClient &client) {
  // Sofortfütterung asynchron markieren und sofort redirecten
  requestFeedFromWebAsync();
  client.print(F("HTTP/1.1 303 See Other\r\n"));
  client.print(F("Location: /"));
  client.print(authQuerySuffix());
  client.print(F("\r\nConnection: close\r\n\r\n"));
}

// Sehr einfache URL-Form-Parser für application/x-www-form-urlencoded
// WICHTIG: Volles URL-Decoding (vorher nur "+" und "%3A") – sonst werden
// WLAN-Passwörter mit Sonderzeichen (&, =, %, +, etc.) falsch gespeichert.
static bool kvFind(const String &body, const String &key, String &out) {
  // Suche nach "key=" entweder am Anfang oder nach einem "&"
  String needle = key + "=";
  int p = body.indexOf(needle);
  // Sicherheit: muss am Anfang oder nach "&" stehen, sonst Substring-Match (z.B. "ap" in "lastap")
  while (p > 0 && body.charAt(p - 1) != '&') {
    p = body.indexOf(needle, p + 1);
  }
  if (p < 0) return false;
  p += needle.length();
  int e = body.indexOf('&', p);
  if (e < 0) e = body.length();
  out = urlDecode(body.substring(p, e));
  return true;
}

static uint32_t toUInt32(const String &s, uint32_t defv) {
  long v = s.toInt();
  if (v < 0) return defv;
  if (v > 600) v = 600;  // Max. 10 Minuten Limit
  return (uint32_t)v;
}

static void handleSave(WiFiClient &client, const String &body) {
  // Felder auslesen (robustheit: Defaults beibehalten, wenn Feld fehlt)
  String sh1, sm1, sh2, sm2, sa2, ssec;
  int h1 = cfgGetH1();
  int m1 = cfgGetM1();
  int h2 = cfgGetH2();
  int m2 = cfgGetM2();
  bool a2 = cfgGetActive2();

  if (kvFind(body, "h1", sh1)) h1 = sh1.toInt();
  if (kvFind(body, "m1", sm1)) m1 = sm1.toInt();
  if (kvFind(body, "h2", sh2)) h2 = sh2.toInt();
  if (kvFind(body, "m2", sm2)) m2 = sm2.toInt();
  a2 = body.indexOf("a2=") >= 0; // Checkbox gesetzt → enthalten
  // Sekunden -> Schritte umrechnen: steps = sec * FEED_STEPS_PER_SEC
  int secVal = cfgGetSteps() / (FEED_STEPS_PER_SEC > 0 ? FEED_STEPS_PER_SEC : 1000);
  if (kvFind(body, "sec", ssec)) {
    uint32_t v = toUInt32(ssec, 5);
    if (v >= 1 && v <= 600) secVal = v;
  }
  // Sekunden in Steps umrechnen (bei 1000 Steps/s)
  uint32_t newSteps = secVal * (FEED_STEPS_PER_SEC > 0 ? FEED_STEPS_PER_SEC : 1000);

  // In Konfiguration übernehmen und speichern
  cfgUpdateAndSave((uint8_t)h1, (uint8_t)m1, (uint8_t)h2, (uint8_t)m2, a2, newSteps);

  // 303 Redirect zurück auf Startseite (verhindert doppeltes Absenden)
  client.print(F("HTTP/1.1 303 See Other\r\n"));
  client.print(F("Location: /"));
  client.print(authQuerySuffix());
  client.print(F("\r\nConnection: close\r\n\r\n"));
}

// Extrahiert Pfad aus Request-Line (ohne Query-String)
// "GET /save?x=1 HTTP/1.1" → "/save"
static String extractPath(const String &reqLine) {
  int spStart = reqLine.indexOf(' ');
  if (spStart < 0) return "";
  int spEnd = reqLine.indexOf(' ', spStart + 1);
  if (spEnd < 0) spEnd = reqLine.length();
  String url = reqLine.substring(spStart + 1, spEnd);
  int q = url.indexOf('?');
  if (q >= 0) url = url.substring(0, q);
  return url;
}

void webHandleClient() {
  WiFiClient client = server.available();
  if (!client) return;

  // Request-Zeile
  client.setTimeout(500);  // schneller Abbruch bei langsamen Clients
  String reqLine = client.readStringUntil('\n');
  reqLine.trim();
  // Header lesen: Content-Length extrahieren, bis Leerzeile (max. 500ms)
  int contentLength = -1;
  unsigned long t0 = millis();
  while (client.connected() && (millis() - t0) < 500UL) {
    String h = client.readStringUntil('\n');
    if (h.length() == 0) continue;
    h.trim();
    if (h.length() == 0) break; // leere Zeile = Ende Header
    if (h.startsWith("Content-Length:")) {
      String v = h.substring(15);
      v.trim();
      contentLength = v.toInt();
      // Schutz: max. 2KB Body
      if (contentLength > 2048) contentLength = 2048;
    }
  }

  // Captive Portal Detection: OS sendet diese URLs um Internet zu testen.
  // Wir antworten mit 302-Redirect zur Setup-Seite → Browser bleibt offen statt sich zu schließen.
  // WICHTIG: Nicht mit echtem Inhalt antworten – das würde das Pop-up sofort schließen.
  if (wifiIsSetupMode()) {
    bool isCaptiveCheck =
        reqLine.indexOf("/generate_204") >= 0 ||        // Android
        reqLine.indexOf("/gen_204") >= 0 ||             // Android
        reqLine.indexOf("/hotspot-detect.html") >= 0 || // iOS
        reqLine.indexOf("/library/test/success.html") >= 0 || // iOS/macOS
        reqLine.indexOf("/success.txt") >= 0 ||         // iOS
        reqLine.indexOf("/ncsi.txt") >= 0 ||            // Windows
        reqLine.indexOf("/connecttest.txt") >= 0 ||     // Windows
        reqLine.indexOf("/redirect") >= 0 ||            // Windows
        reqLine.indexOf("/canonical.html") >= 0;        // Firefox/Ubuntu
    if (isCaptiveCheck) {
      client.print(F("HTTP/1.1 302 Found\r\n"));
      client.print(F("Location: http://192.168.4.1/\r\n"));
      client.print(F("Connection: close\r\n\r\n"));
      delay(1);
      client.stop();
      return;
    }
  }

  // Body fuer alle POST-Requests upfront lesen (vereinheitlicht Auth + Handler)
  bool isPost = reqLine.startsWith("POST ");
  String body;
  if (isPost) {
    if (contentLength < 0) contentLength = 0;
    // Reserve mind. 512 Bytes oder Content-Length (vermeidet Heap-Fragmentation)
    body.reserve((unsigned)((contentLength > 512) ? contentLength : 512));
    unsigned long dlBody = millis() + 500UL;  // 500ms Body-Timeout
    while ((int)body.length() < contentLength && millis() < dlBody) {
      while (client.available() && (int)body.length() < contentLength) body += (char)client.read();
      delay(1);
    }
  }

  String path = extractPath(reqLine);

  // Auth-Check (zentral): nur im Heimnetz-Modus, ausgenommen Manifest und Captive-Portal-Wifisetup
  // Im Setup-Modus gibt es kein Admin-Passwort, weil noch nichts gespeichert ist.
  gCurrentAuthPass = "";
  gCurrentAuthPassHtml = "";
  gCurrentAuthPassUrl = "";
  if (!wifiIsSetupMode() && path != "/manifest.json") {
    if (!requireAuth(client, reqLine, body)) {
      delay(1);
      client.stop();
      return;
    }
    // Auth bestanden: Passwort + gecachte Varianten fuer Form/URL-Einbettung merken
    gCurrentAuthPass = extractAdminPass(reqLine, body);
    if (gCurrentAuthPass.length() > 0) {
      gCurrentAuthPassHtml = htmlEscape(gCurrentAuthPass);
      gCurrentAuthPassUrl = urlEncode(gCurrentAuthPass);
    }
  }

  if (path == "/" && reqLine.startsWith("GET")) {
    handleRoot(client);
  } else if (path == "/feed" && isPost) {
    handleFeed(client);
  } else if (path == "/wifisetup" && isPost) {
    String sSSID, sPASS, sAdminPass;
    if (kvFind(body, "ssid", sSSID) && kvFind(body, "pass", sPASS)) {
      kvFind(body, "admin_pass", sAdminPass);  // optional, kann leer sein
      wifiCredSave(sSSID.c_str(), sPASS.c_str(), sAdminPass.c_str());
    }
    client.print(F("HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nConnection: close\r\n\r\n"));
    client.print(F("<!DOCTYPE html><html lang=\"de\"><head><meta charset=\"utf-8\">"));
    client.print(F("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"));
    client.print(F("<title>K.O.R.N. verbindet...</title>"));
    client.print(F("<style>body{font-family:-apple-system,BlinkMacSystemFont,Segoe UI,Roboto,sans-serif;margin:32px 16px;text-align:center;background:#f5f5f5}"));
    client.print(F(".card{background:#fff;border-radius:12px;padding:32px 24px;box-shadow:0 2px 8px rgba(0,0,0,0.1)}"));
    client.print(F("h1{font-size:1.4rem;margin:12px 0}p{font-size:16px;color:#444;margin:8px 0}</style></head><body>"));
    client.print(F("<div class=\"card\">"));
    client.print(F("<div style=\"font-size:3rem\">✅</div>"));
    client.print(F("<h1>WLAN-Daten gespeichert!</h1>"));
    client.print(F("<p>Arduino startet neu und verbindet sich mit dem Heimnetz.</p>"));
    client.print(F("<p style=\"margin-top:20px;font-weight:600\">Danach erreichbar unter:</p>"));
    client.print(F("<p style=\"font-size:20px;color:#1976d2;font-weight:700;margin:4px 0\">http://korn</p>"));
    client.print(F("<p style=\"color:#888;font-size:14px;margin-top:12px\">Falls nicht: IP aus Router-Oberfläche (Gerätename \"KORN\")</p>"));
    client.print(F("</div></body></html>"));
    delay(500);
    client.stop();
    delay(200);
    NVIC_SystemReset(); // Arduino UNO R4 Neustart
  } else if (path == "/setup" && !isPost) {
    // Einrichtungsseite für WLAN-Konfiguration (PWA-optimiert)
    client.print(F("HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nConnection: close\r\n\r\n"));
    client.print(F("<!DOCTYPE html><html lang=\"de\"><head><meta charset=\"utf-8\">"));
    client.print(F("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1, viewport-fit=cover\">"));
    client.print(F("<meta name=\"theme-color\" content=\"#1976d2\">"));
    client.print(F("<meta name=\"apple-mobile-web-app-capable\" content=\"yes\">"));
    client.print(F("<meta name=\"apple-mobile-web-app-status-bar-style\" content=\"black-translucent\">"));
    client.print(F("<meta name=\"apple-mobile-web-app-title\" content=\"K.O.R.N. Setup\">"));
    client.print(F("<link rel=\"manifest\" href=\"/manifest.json\">"));
    client.print(F("<title>K.O.R.N. WLAN-Einrichtung</title>"));
    client.print(F("<style>"));
    client.print(F("body{font-family:-apple-system,BlinkMacSystemFont,Segoe UI,Roboto,Helvetica,Arial,sans-serif;margin:16px;line-height:1.5;background:#f5f5f5}"));
    client.print(F(".card{background:#fff;border-radius:12px;padding:24px;margin:16px 0;box-shadow:0 2px 8px rgba(0,0,0,0.1)}"));
    client.print(F("input{width:100%;font-size:20px;margin:12px 0;padding:16px;border:2px solid #ddd;border-radius:8px;box-sizing:border-box;min-height:56px}"));
    client.print(F("input:focus{border-color:#1976d2;outline:none}"));
    client.print(F("button{width:100%;padding:18px;font-size:18px;font-weight:600;border:none;border-radius:8px;min-height:56px;background:#1976d2;color:#fff;cursor:pointer;margin-top:16px}"));
    client.print(F("button:active{background:#0d47a1;transform:scale(0.98)}"));
    client.print(F(".btn-secondary{background:#757575}"));
    client.print(F(".btn-secondary:active{background:#424242}"));
    client.print(F(".header{display:flex;align-items:center;justify-content:space-between;margin-bottom:8px}"));
    client.print(F(".close-btn{font-size:28px;text-decoration:none;color:#666;padding:8px}"));
    client.print(F("h1{font-size:1.5rem;margin:0}"));
    client.print(F("</style></head><body>"));
    client.print(F("<div class=\"card\">"));
    client.print(F("<div class=\"header\"><h1>🔧 WLAN-Einrichtung</h1><a href=\"/\" class=\"close-btn\" title=\"Zurück zur Übersicht\">&times;</a></div>"));
    client.print(F("<p>Bitte Zugangsdaten des Heimnetzes eingeben:</p>"));
    client.print(F("<form method=\"POST\" action=\"/wifisetup\">"));
    client.print(F("<label>WLAN-Name (SSID):<input type=\"text\" name=\"ssid\" autocomplete=\"off\" placeholder=\"MeinHeimnetz\" required></label>"));
    client.print(F("<label>Passwort:<input type=\"password\" name=\"pass\" autocomplete=\"off\" placeholder=\"WLAN-Passwort (min. 8 Zeichen)\" minlength=\"8\" maxlength=\"63\" required></label>"));
    client.print(F("<hr style=\"margin:16px 0;border-color:#ddd\">"));
    client.print(F("<label>🔒 Admin-Passwort (optional):<input type=\"password\" name=\"admin_pass\" autocomplete=\"off\" placeholder=\"Web-UI schützen (leer lassen = kein Schutz)\" minlength=\"4\" maxlength=\"32\"></label>"));
    client.print(F("<div style=\"font-size:13px;color:#666;margin:-8px 0 12px 0\">Falls gesetzt, wird bei jedem Zugriff auf KORN nach diesem Passwort gefragt.</div>"));
    client.print(F("<button type=\"submit\">💾 Speichern & Verbinden</button>"));
    client.print(F("</form>"));
    client.print(F("<a href=\"/\"><button class=\"btn-secondary\">❌ Abbrechen / Zurück</button></a>"));
    client.print(F("</div></body></html>"));
  } else if ((path == "/jogcw" || path == "/jogccw") && isPost) {
    String sSec;
    int seconds = 2;  // Default 2 Sekunden
    if (kvFind(body, "sec2", sSec)) {
      uint32_t v = toUInt32(sSec, 2);
      if (v >= 1 && v <= 60) seconds = (int)v;  // 1-60 Sekunden
    }
    // Sekunden in Steps umrechnen (bei 1000 steps/sec)
    int steps = seconds * FEED_STEPS_PER_SEC;
    bool dirCW = (path == "/jogcw");
    
    Serial.print(F("Blockade lösen: "));
    Serial.print(seconds);
    Serial.print(F(" Sekunden = "));
    Serial.print(steps);
    Serial.println(F(" Steps"));
    
    // Ausführen (blockierend, wie feed), nutzt Relais/Buzzer aus motorFeed
    motorFeed(steps, dirCW);
    // Redirect zurück
    client.print(F("HTTP/1.1 303 See Other\r\n"));
    client.print(F("Location: /"));
    client.print(authQuerySuffix());
    client.print(F("\r\nConnection: close\r\n\r\n"));
  } else if (path == "/log" && !isPost) {
    logRenderHTML(client);
  } else if (path == "/status" && !isPost) {
    handleStatus(client);
  } else if (path == "/save" && isPost) {
    handleSave(client, body);
  } else if (path == "/reset" && isPost) {
    // Factory Reset: EEPROM löschen und neu starten
    client.print(F("HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nConnection: close\r\n\r\n"));
    client.print(F("<!DOCTYPE html><html lang=\"de\"><head><meta charset=\"utf-8\"></head><body>"));
    client.print(F("<h2>🗑️ Werkseinstellungen werden wiederhergestellt...</h2>"));
    client.print(F("<p>EEPROM wird gelöscht. Arduino startet neu.</p>"));
    client.print(F("</body></html>"));
    client.stop();
    delay(500);
    // EEPROM löschen (1024 Bytes beim UNO R4)
    for (int i = 0; i < 1024; i++) {
      EEPROM.write(i, 0xFF);
    }
    Serial.println(F("Factory Reset: EEPROM gelöscht."));
    delay(200);
    NVIC_SystemReset(); // Arduino UNO R4 Neustart
  } else if (path == "/manifest.json" && !isPost) {
    // Web App Manifest für PWA-Support
    client.print(F("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nConnection: close\r\n\r\n"));
    client.print(F("{\"name\":\"K.O.R.N. Fütterer\",\"short_name\":\"K.O.R.N.\",\"description\":\"Katastrophal Organisierter Runder Nahrungsmittelspender\",\"start_url\":\"/\",\"display\":\"standalone\",\"background_color\":\"#f5f5f5\",\"theme_color\":\"#1976d2\",\"icons\":[{\"src\":\"data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 192 192'%3E%3Crect fill='%231976d2' width='192' height='192'/%3E%3Ctext x='96' y='120' font-size='100' text-anchor='middle' fill='white'%3E🐔%3C/text%3E%3C/svg%3E\",\"sizes\":\"192x192\",\"type\":\"image/svg+xml\"}]}"));
  } else {
    // Fallback
    handleRoot(client);
  }

  delay(1);
  client.stop();
}
