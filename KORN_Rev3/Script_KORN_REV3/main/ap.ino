// ap.ino – Access Point & Netzwerk
#include <Arduino.h>
#include <WiFiS3.h>

// Globale Serverinstanz (Port 80)
WiFiServer server(80);

static const IPAddress DEFAULT_AP_IP(192, 168, 4, 1); // WiFiS3 AP nutzt i.d.R. 192.168.4.1

void apInit(const char* ssid, const char* pass) {
  // AP starten
  WiFi.end();
  delay(100);
  int status = WiFi.beginAP(ssid, pass);
  (void)status;
  delay(500);

  // Server starten
  server.begin();

  // Serielle Kurzinfo
  IPAddress ip = WiFi.localIP();
  Serial.print(F("AP gestartet: ")); Serial.print(ssid);
  Serial.print(F("  IP: ")); Serial.println(ip);
  // Hinweis: Viele AP-Implementierungen nutzen 192.168.4.1 automatisch.
  // Explizites Setzen der AP-IP wird von WiFiS3 ggf. nicht unterstützt.
  // Wir loggen sie daher verlässlich, damit die URL eindeutig ist.
}

static bool lastApWasOk = true;

void apCheckAndReconnect() {
  // WiFi-Status prüfen
  int status = WiFi.status();
  bool apOk = (status == WL_AP_LISTENING || status == WL_AP_CONNECTED);
  
  // Nur bei Statuswechsel von OK zu nicht-OK reconnecten
  if (!apOk && lastApWasOk) {
    Serial.print(F("WLAN-Verbindung verloren (Status="));
    Serial.print(status);
    Serial.println(F(") - Reconnect..."));
    
    // AP neu starten
    WiFi.end();
    delay(200);
    
    int newStatus = WiFi.beginAP("KORN", "Chaosfeeder");
    delay(500);
    
    // Server neu starten
    server.begin();
    
    IPAddress ip = WiFi.localIP();
    Serial.print(F("AP wiederhergestellt: IP="));
    Serial.println(ip);
  }
  
  lastApWasOk = apOk;
}
