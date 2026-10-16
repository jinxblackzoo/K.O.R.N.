// wifi_client.ino – WLAN-Client + Ersteinrichtungs-AP-Fallback
// Ersetzt ap.ino aus Rev3.
//
// Logik:
//  1) WLAN-Zugangsdaten aus EEPROM laden
//  2) Verbindung mit Heimnetz versuchen (Timeout: WIFI_CONNECT_TIMEOUT_MS)
//  3) Falls fehlgeschlagen: Einrichtungs-AP "KORN-Setup" öffnen
//     -> Einfache Konfigurationsseite unter 192.168.4.1/setup
//     -> Nach Speichern: Neustart, dann Heimnetz-Verbindung
//  4) NTP-Sync nach erfolgreicher Heimnetz-Verbindung (in main.ino)

#include <Arduino.h>
#include <WiFiS3.h>
#include <WiFiUdp.h>
#include <EEPROM.h>

// Serverinstanz und WifiCred-Struct werden in main.ino definiert (Kompilierreihenfolge)
extern WiFiServer server;

// DNS-Server für Captive Portal (leitet alle Anfragen auf 192.168.4.1)
static WiFiUDP dnsUDP;
static const uint16_t DNS_PORT = 53;
static uint8_t dnsBuf[256];

// Access-Point-Zugangsdaten – gilt für Einrichtungs- UND Service-AP.
// WICHTIG: Der UNO R4 (WiFiS3) spannt nur MIT Passwort (WPA2, min. 8 Zeichen)
// zuverlässig einen AP auf. Offene APs funktionieren NICHT stabil.
static const char* SETUP_AP_SSID = "KORN-Setup";
static const char* SETUP_AP_PASS = "12345678";   // min. 8 Zeichen erforderlich!

// Verbindungs-Timeout in Millisekunden
static const uint32_t WIFI_CONNECT_TIMEOUT_MS = 15000UL;

// Betriebsmodell (Standorte mit schwachem WLAN):
//  - Steady State: eigener AP mit Homepage (immer lokal erreichbar).
//  - Periodisch kurz ins Heimnetz wechseln, NTP holen, zurück in den AP.
//    (UNO R4 / WiFiS3 kann AP und Client NICHT gleichzeitig → Wechselbetrieb.)
static const uint32_t NTP_REFRESH_INTERVAL_MS = 12UL * 3600UL * 1000UL; // 12h: NTP nachholen
static const uint32_t NTP_RETRY_NOSYNC_MS     = 600000UL;               // 10 min, solange noch nie NTP

// Forward-Declarations (in main.ino definiert)
void logEvent(const char* msg);
bool ntpSyncBlocking();   // führt einen NTP-Sync aus (WLAN muss verbunden sein)
bool ntpIsSynced();       // true sobald jemals NTP synchronisiert wurde

// Zustandsvariablen
static bool gIsSetupMode = false;   // true = Einrichtungs-AP (keine WLAN-Daten, Captive Portal)
static bool gIsServiceAP = false;   // true = Service-AP mit Homepage (WLAN-Daten vorhanden)
static bool gWifiConnected = false; // true = aktuell im Heimnetz (nur kurz während NTP-Sync)
static char gConnectedSSID[33] = "";
static char gLocalIP[16] = "";      // IP als String (z.B. "192.168.1.42")
static unsigned long gLastNtpRefreshMs = 0; // millis() des letzten NTP-Refresh-Versuchs

// Vorwärtsdeklarationen
static void startServiceAP();
static void wifiNtpRefreshCycle();

// Liefert EEPROM-Startadresse für WifiCred (hinter KConfig)
static int wifiCredOffset() {
  return (int)sizeof(KConfig) + 4; // 4 Bytes Puffer
}

// WLAN-Zugangsdaten aus EEPROM laden
static bool wifiCredLoad(WifiCred &cred) {
  EEPROM.get(wifiCredOffset(), cred);
  return (cred.magic == WIFI_CRED_MAGIC &&
          cred.ssid[0] != '\0' &&
          strlen(cred.ssid) <= 32);
}

// WLAN-Zugangsdaten in EEPROM speichern
void wifiCredSave(const char* ssid, const char* pass, const char* admin_pass) {
  WifiCred cred;
  cred.magic = WIFI_CRED_MAGIC;
  strncpy(cred.ssid, ssid, sizeof(cred.ssid) - 1);
  cred.ssid[sizeof(cred.ssid) - 1] = '\0';
  strncpy(cred.pass, pass, sizeof(cred.pass) - 1);
  cred.pass[sizeof(cred.pass) - 1] = '\0';
  if (admin_pass && strlen(admin_pass) > 0) {
    strncpy(cred.admin_pass, admin_pass, sizeof(cred.admin_pass) - 1);
    cred.admin_pass[sizeof(cred.admin_pass) - 1] = '\0';
  } else {
    cred.admin_pass[0] = '\0';  // leer = kein Schutz
  }
  EEPROM.put(wifiCredOffset(), cred);
  Serial.println(F("WLAN-Zugangsdaten gespeichert."));
}

// Prüft ob Admin-Passwort gesetzt ist
bool wifiCredHasAdminPass() {
  WifiCred cred;
  if (!wifiCredLoad(cred)) return false;
  return cred.admin_pass[0] != '\0';
}

// Prüft ob übergebenes Admin-Passwort korrekt ist (oder keins gesetzt)
bool wifiCredCheckAdminPass(const char* input) {
  WifiCred cred;
  if (!wifiCredLoad(cred)) return true;  // kein Cred = kein Schutz
  if (cred.admin_pass[0] == '\0') return true;  // kein Admin-Passwort gesetzt
  if (!input) return false;
  return strcmp(cred.admin_pass, input) == 0;
}

// Gibt zurück ob Einrichtungs-AP aktiv ist (Erstinbetriebnahme, keine Credentials)
bool wifiIsSetupMode() {
  return gIsSetupMode;
}

// Gibt zurück ob der Fallback-AP aktiv ist (Heimnetz verloren, Credentials vorhanden)
bool wifiIsFallbackAP() {
  return gIsFallbackAP;
}

// Gibt zurück ob irgendein eigener Access Point aktiv ist (Setup oder Fallback)
bool wifiApActive() {
  return gIsSetupMode || gIsFallbackAP;
}

// Gibt zurück ob Heimnetz verbunden ist
bool wifiIsConnected() {
  return gWifiConnected;
}

// Aktuell verbundene SSID
const char* wifiGetSSID() {
  return gConnectedSSID;
}

// Einrichtungs-AP starten (Erstinbetriebnahme, keine WLAN-Zugangsdaten)
static void startSetupAP() {
  gIsSetupMode = true;
  gIsFallbackAP = false;
  gWifiConnected = false;
  WiFi.end();
  delay(200);
  WiFi.beginAP(SETUP_AP_SSID);  // offener AP (kein Passwort)
  delay(500);
  server.begin();
  // DNS-Server starten (Captive Portal: leitet alle Domains auf AP-IP)
  dnsUDP.begin(DNS_PORT);
  IPAddress ip = WiFi.localIP();
  snprintf(gLocalIP, sizeof(gLocalIP), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
  Serial.print(F("Einrichtungs-AP gestartet: "));
  Serial.print(SETUP_AP_SSID);
  Serial.print(F("  IP: "));
  Serial.println(ip);
  Serial.println(F("Captive Portal aktiv – Browser öffnet sich automatisch"));
}

// Fallback-AP starten (Heimnetz verloren, aber WLAN-Zugangsdaten vorhanden)
// Unterschied zu startSetupAP(): gIsSetupMode bleibt false, damit die Web-UI direkt
// die NORMALE Status-/Konfig-Seite (Einstellungen) zeigt statt der Ersteinrichtung.
// Im Fallback-AP wird die Admin-Auth bewusst übersprungen (offener AP ohne Passwort).
// wifiCheckAndReconnect() versucht aus diesem Modus heraus periodisch das Heimnetz.
static void startFallbackAP() {
  gIsFallbackAP = true;
  gIsSetupMode = false;
  gWifiConnected = false;
  WiFi.end();
  delay(200);
  // OFFENER AP (kein Passwort) – beginAP(ssid) ohne Passphrase.
  // So kommt man am abgelegenen Standort ohne Passwort schnell an die Einstellungen.
  WiFi.beginAP(SETUP_AP_SSID);
  delay(500);
  server.begin();
  dnsUDP.begin(DNS_PORT);
  gApSinceMs = millis();
  IPAddress ip = WiFi.localIP();
  snprintf(gLocalIP, sizeof(gLocalIP), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
  Serial.print(F("Fallback-AP gestartet (offen, Heimnetz verloren): "));
  Serial.print(SETUP_AP_SSID);
  Serial.print(F("  IP: "));
  Serial.println(ip);
  logEvent("Fallback-AP gestartet (offen)");
}

// DNS-Anfragen verarbeiten (Captive Portal)
// Alle DNS-Queries werden mit der AP-IP (192.168.4.1) beantwortet
void dnsHandleRequests() {
  if (!wifiApActive()) return;  // läuft im Setup- UND im Fallback-AP
  int packetSize = dnsUDP.parsePacket();
  if (packetSize < 12 || packetSize > (int)sizeof(dnsBuf)) return;
  int len = dnsUDP.read(dnsBuf, sizeof(dnsBuf));
  if (len < 12) return;
  IPAddress remoteIP = dnsUDP.remoteIP();
  uint16_t remotePort = dnsUDP.remotePort();

  // DNS-Antwort bauen: Header-Flags setzen, ANCOUNT=1
  dnsBuf[2] = 0x81; // QR=1, OPCODE=0, AA=1, TC=0, RD=1
  dnsBuf[3] = 0x80; // RA=1, Z=0, RCODE=0
  dnsBuf[6] = 0x00; dnsBuf[7] = 0x01; // ANCOUNT = 1
  dnsBuf[8] = 0x00; dnsBuf[9] = 0x00; // NSCOUNT = 0
  dnsBuf[10] = 0x00; dnsBuf[11] = 0x00; // ARCOUNT = 0

  // Antwort an Ende anhängen
  int pos = len;
  // Name: Pointer auf Offset 12 (Beginn Question Name)
  if (pos + 16 > (int)sizeof(dnsBuf)) return;
  dnsBuf[pos++] = 0xC0; dnsBuf[pos++] = 0x0C;
  // Type A
  dnsBuf[pos++] = 0x00; dnsBuf[pos++] = 0x01;
  // Class IN
  dnsBuf[pos++] = 0x00; dnsBuf[pos++] = 0x01;
  // TTL = 60
  dnsBuf[pos++] = 0x00; dnsBuf[pos++] = 0x00;
  dnsBuf[pos++] = 0x00; dnsBuf[pos++] = 0x3C;
  // RDLENGTH = 4
  dnsBuf[pos++] = 0x00; dnsBuf[pos++] = 0x04;
  // RDATA = 192.168.4.1
  dnsBuf[pos++] = 192; dnsBuf[pos++] = 168;
  dnsBuf[pos++] = 4;   dnsBuf[pos++] = 1;

  dnsUDP.beginPacket(remoteIP, remotePort);
  dnsUDP.write(dnsBuf, pos);
  dnsUDP.endPacket();
}

// Heimnetz-Verbindung herstellen
// WICHTIG: Watchdog während der Wait-Schleifen refreshen, sonst Boot-Loop bei langsamem WLAN
static bool connectToHome(const char* ssid, const char* pass) {
  Serial.print(F("Verbinde mit WLAN: "));
  Serial.println(ssid);
  WiFi.end();
  delay(200);
  watchdogRefresh();
  WiFi.setHostname("KORN"); // DHCP Option 12: Router zeigt Gerät als "KORN" an
  WiFi.begin(ssid, pass);
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - start > WIFI_CONNECT_TIMEOUT_MS) {
      Serial.println(F("WLAN-Verbindung Timeout."));
      return false;
    }
    watchdogRefresh();
    delay(500);
    Serial.print('.');
  }
  Serial.println();
  // Auf gültige DHCP-IP warten (max. 5s) – verhindert IP=0.0.0.0 Logs
  unsigned long ipStart = millis();
  while (millis() - ipStart < 5000UL) {
    watchdogRefresh();
    IPAddress ip = WiFi.localIP();
    if (ip[0] != 0 || ip[1] != 0 || ip[2] != 0 || ip[3] != 0) break;
    delay(200);
  }
  return true;
}

// Initialisierung: Heimnetz oder Einrichtungs-AP
// Bei gespeicherten Credentials werden bis zu 3 Verbindungsversuche unternommen.
// Falls alle fehlschlagen → Setup-AP als Rückfall-Option (sonst wäre das Gerät
// bei Tippfehler im WLAN-Passwort für immer unerreichbar).
void wifiInit() {
  WifiCred cred;
  if (wifiCredLoad(cred)) {
    for (int attempt = 1; attempt <= 3; attempt++) {
      Serial.print(F("WLAN-Verbindungsversuch "));
      Serial.print(attempt);
      Serial.println(F("/3"));
      if (connectToHome(cred.ssid, cred.pass)) {
        gIsSetupMode = false;
        gWifiConnected = true;
        strncpy(gConnectedSSID, cred.ssid, sizeof(gConnectedSSID) - 1);
        server.begin();
        IPAddress ip = WiFi.localIP();
        Serial.print(F("WLAN verbunden. IP: "));
        Serial.println(ip);
        return;
      }
      // Kurz warten zwischen Versuchen – mit Watchdog-Refresh
      for (int w = 0; w < 4; w++) { watchdogRefresh(); delay(500); }
    }
    // Alle Versuche beim Boot fehlgeschlagen, ABER es sind Zugangsdaten gespeichert.
    // → Fallback-AP (NICHT Setup-AP), damit wifiCheckAndReconnect() weiterhin
    //   periodisch das Heimnetz versucht. Verhindert die Dauerfalle bei schwachem
    //   WLAN nach einem Stromausfall (sonst würde KORN nie wieder verbinden/füttern).
    Serial.println(F("WLAN-Verbindung beim Boot fehlgeschlagen – starte Fallback-AP (Reconnect läuft weiter)."));
    startFallbackAP();
    gDisconnectedSinceMs = millis();
    return;
  }
  Serial.println(F("Keine WLAN-Zugangsdaten gespeichert."));
  // Fallback: Einrichtungs-AP (Erstinbetriebnahme)
  startSetupAP();
}

// Reconnect-Prüfung (periodisch aus main.ino aufrufen)
// Strategie:
//  - Setup-AP (keine Credentials): nichts zu tun – echte Erstinbetriebnahme.
//  - Fallback-AP (Credentials vorhanden): alle AP_RETRY_HOME_MS das Heimnetz
//    erneut testen (AP wird dafür kurz beendet). Erfolg → zurück in STA-Modus.
//  - STA-Modus: bei Verlust reconnecten; nach AP_FALLBACK_AFTER_MS ohne Erfolg
//    → Fallback-AP aufspannen (Web-UI bleibt lokal erreichbar).
// HINWEIS: connectToHome() kann > 4s blockieren (WiFi.begin auf UNO R4). Falls das
// passiert, löst der Watchdog einen Reboot aus → setup() läuft erneut, wifiInit()
// greift. Das ist als Selbstheilung akzeptabel.
// WICHTIG: Geplante/Not-Fütterungen laufen unabhängig vom WLAN weiter
// (Scheduler über millis()-Drift, Notfütterung ohne NTP).
void wifiCheckAndReconnect() {
  // Echte Erstinbetriebnahme: kein Reconnect möglich (kein Ziel-Netz bekannt).
  if (gIsSetupMode) return;

  WifiCred cred;
  bool haveCred = wifiCredLoad(cred);

  if (gIsFallbackAP) {
    // Aus dem Fallback-AP heraus periodisch das Heimnetz versuchen.
    if (haveCred && (millis() - gApSinceMs) >= AP_RETRY_HOME_MS) {
      Serial.println(F("Fallback-AP: erneuter Heimnetz-Versuch..."));
      if (connectToHome(cred.ssid, cred.pass)) {
        gIsFallbackAP = false;
        gWifiConnected = true;
        gDisconnectedSinceMs = 0;
        strncpy(gConnectedSSID, cred.ssid, sizeof(gConnectedSSID) - 1);
        dnsUDP.stop();
        server.begin();
        IPAddress ip = WiFi.localIP();
        Serial.print(F("Heimnetz wieder verbunden (aus Fallback-AP). IP: "));
        Serial.println(ip);
        logEvent("Reconnect aus Fallback-AP ok");
      } else {
        // Weiterhin kein Heimnetz → Fallback-AP erneut aufspannen (Timer neu setzen)
        startFallbackAP();
      }
    }
    return;
  }

  // STA-Modus
  if (WiFi.status() != WL_CONNECTED) {
    gWifiConnected = false;
    if (gDisconnectedSinceMs == 0) gDisconnectedSinceMs = millis();
    Serial.println(F("WLAN getrennt – Reconnect..."));
    if (!haveCred) {
      // Keine Credentials → echte Erstinbetriebnahme
      Serial.println(F("Keine WLAN-Daten – Einrichtungs-AP gestartet."));
      startSetupAP();
      return;
    }
    if (connectToHome(cred.ssid, cred.pass)) {
      gWifiConnected = true;
      gDisconnectedSinceMs = 0;
      server.begin();
      IPAddress ip = WiFi.localIP();
      Serial.print(F("WLAN wiederverbunden. IP: "));
      Serial.println(ip);
    } else if ((millis() - gDisconnectedSinceMs) >= AP_FALLBACK_AFTER_MS) {
      // Heimnetz schon länger weg → Fallback-AP für lokalen Web-UI-Zugriff.
      Serial.println(F("Heimnetz lange unerreichbar – starte Fallback-AP."));
      logEvent("WLAN lange weg -> Fallback-AP");
      startFallbackAP();
    } else {
      Serial.println(F("Reconnect fehlgeschlagen – nächster Versuch in 30s."));
    }
  } else {
    gWifiConnected = true;
    gDisconnectedSinceMs = 0;
  }
}

