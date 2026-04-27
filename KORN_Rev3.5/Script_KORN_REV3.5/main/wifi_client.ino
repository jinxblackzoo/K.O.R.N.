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

// Einrichtungs-AP Zugangsdaten
static const char* SETUP_AP_SSID = "KORN-Setup";
static const char* SETUP_AP_PASS = "Chaosfeeder";

// Verbindungs-Timeout in Millisekunden
static const uint32_t WIFI_CONNECT_TIMEOUT_MS = 15000UL;

// Zustandsvariablen
static bool gIsSetupMode = false;   // true = Einrichtungs-AP aktiv
static bool gWifiConnected = false; // true = Heimnetz verbunden
static char gConnectedSSID[33] = "";
static char gLocalIP[16] = "";      // IP als String (z.B. "192.168.1.42")

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

// Gibt zurück ob Einrichtungs-AP aktiv ist
bool wifiIsSetupMode() {
  return gIsSetupMode;
}

// Gibt zurück ob Heimnetz verbunden ist
bool wifiIsConnected() {
  return gWifiConnected;
}

// Aktuell verbundene SSID
const char* wifiGetSSID() {
  return gConnectedSSID;
}

// Einrichtungs-AP starten
static void startSetupAP() {
  gIsSetupMode = true;
  gWifiConnected = false;
  WiFi.end();
  delay(200);
  WiFi.beginAP(SETUP_AP_SSID, SETUP_AP_PASS);
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

// DNS-Anfragen verarbeiten (Captive Portal)
// Alle DNS-Queries werden mit der AP-IP (192.168.4.1) beantwortet
void dnsHandleRequests() {
  if (!gIsSetupMode) return;
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
    // Alle Versuche beim Boot fehlgeschlagen → Setup-AP anbieten.
    // So kommt der Nutzer bei Tippfehler im WLAN-PW wieder rein.
    Serial.println(F("WLAN-Verbindung beim Boot fehlgeschlagen – starte Setup-AP als Rückfall."));
    startSetupAP();
    return;
  }
  Serial.println(F("Keine WLAN-Zugangsdaten gespeichert."));
  // Fallback: Einrichtungs-AP (Erstinbetriebnahme)
  startSetupAP();
}

// Reconnect-Prüfung (periodisch aus main.ino aufrufen)
// Strategie: Bei vorhandenen WLAN-Daten weiter probieren (kein automatischer Setup-AP).
// Setup-AP nur, wenn keine Credentials gespeichert sind (Erstinbetriebnahme).
// HINWEIS: connectToHome() kann > 4s blockieren (WiFi.begin auf UNO R4). Falls das
// passiert, löst der Watchdog einen Reboot aus → setup() läuft erneut, wifiInit()
// greift. Das ist als Selbstheilung akzeptabel.
void wifiCheckAndReconnect() {
  if (gIsSetupMode) return; // Im Setup-Modus kein Reconnect
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println(F("WLAN getrennt – Reconnect..."));
    gWifiConnected = false;
    WifiCred cred;
    if (!wifiCredLoad(cred)) {
      // Keine Credentials → echte Erstinbetriebnahme
      Serial.println(F("Keine WLAN-Daten – Einrichtungs-AP gestartet."));
      startSetupAP();
      return;
    }
    if (connectToHome(cred.ssid, cred.pass)) {
      gWifiConnected = true;
      server.begin();
      IPAddress ip = WiFi.localIP();
      Serial.print(F("WLAN wiederverbunden. IP: "));
      Serial.println(ip);
    } else {
      // Reconnect fehlgeschlagen – nicht Setup-AP starten, sondern beim nächsten
      // wifiCheckAndReconnect-Aufruf erneut versuchen. Geplante Fütterungen
      // laufen weiter (gNtpSynced bleibt erhalten, Zeit per millis()-Drift).
      Serial.println(F("Reconnect fehlgeschlagen – nächster Versuch in 30s."));
    }
  }
}

