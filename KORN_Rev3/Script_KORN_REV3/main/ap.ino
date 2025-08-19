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
