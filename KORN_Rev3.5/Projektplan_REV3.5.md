# K.O.R.N. Rev3.5 – Projektplan

## Basis
- Basiert auf Rev3 (funktionsfähig, bewährt)
- Gleiche Hardware: Arduino UNO R4 WiFi
- Gleiche Sprache: C++ (Arduino IDE)

## Hardware-Änderungen gegenüber Rev3
- **DS1302 RTC entfällt**: Zeitsteuerung über NTP
- **EEPROM bleibt**: Konfigurationsspeicherung weiterhin per EEPROM
- Stepper-Treiber DM320T, Relais, Buzzer, Taster bleiben unverändert
- Pin-Belegung bleibt gleich (Rev3-Verdrahtung weiter nutzbar)

## CAD
- Gleiche CAD-Änderungen wie Rev4 (Wendel 4.0, DN50_Achsträger_Rev4, Arduino-Träger etc.)
- CAD-Dateien werden aus `KORN_Rev4/cad/` übernommen bzw. referenziert
- Kein eigener CAD-Ordner in Rev3.5 nötig – CAD liegt in Rev4

## Software-Änderungen gegenüber Rev3
- **WLAN-Modus**: Eigener Access Point (AP) → Client im Heimnetz (WLAN-Router)
- **Ersteinrichtungs-AP**: Falls keine WLAN-Zugangsdaten gespeichert oder WLAN nicht erreichbar → Arduino öffnet temporären AP zur Konfiguration per Handy/Browser
- **Zeitquelle**: DS1302 RTC → NTP-Sync aus dem Internet (Zeitzone Europe/Berlin, Sommer-/Winterzeit)
- **ap.ino**: Komplett ersetzt durch WLAN-Client-Logik + Ersteinrichtungs-AP-Fallback
- **main.ino**: DS1302/Makuna-Abhängigkeit entfernt, NTP-Sync integriert
- **motor.ino**: Erweitert um Buzzer-Jagdsignal "Zum Essen" (3/8-Rhythmus) während des Motorlaufs
- **homepage.ino**: Weitgehend übernommen, kleine Anpassungen (IP-Anzeige, NTP-Status)

## Was bleibt gleich
- 2 Fütterungszeiten täglich, konfigurierbar
- Laufzeit 1–600 Sekunden
- 2-Minuten-Mindestabstand zwischen Fütterungen
- "Jetzt füttern"-Button (Web)
- "Blockade lösen (Rechtslauf)"-Button (Web)
- Manueller Hardware-Taster (GPIO-Kurzschluss Pin 10↔11)
- Mobilfreundliche deutschsprachige Web-UI
- Tages-Reset der Fütterungsmarker um Mitternacht
- Buzzer-Signal bei Fütterung
- EEPROM-Konfigurationsspeicherung

## Geplante Dateistruktur
```
KORN_Rev3.5/
├── Projektplan.md
└── Script_KORN_REV3.5/
    └── main/
        ├── main.ino       – Hauptschleife, Orchestrierung, NTP-Zeitlogik
        ├── wifi_client.ino – WLAN-Client + Ersteinrichtungs-AP-Fallback
        ├── motor.ino      – Stepper, Relais, Buzzer (aus Rev3 übernommen)
        ├── homepage.ino   – Web-UI HTML (aus Rev3 angepasst)
        └── (webserver.ino entfällt – HTTP-Routen in homepage.ino integriert)
```

## Entfallende Bibliotheken (gegenüber Rev3)
- ~~Rtc by Makuna~~ (DS1302 entfällt)
- ~~ThreeWire~~ (DS1302 entfällt)

## Neue/bleibende Bibliotheken
- WiFiS3 (vorinstalliert) – jetzt als Client statt AP
- WiFiUdp (vorinstalliert) – für NTP + Captive Portal DNS
- AccelStepper – unverändert
- EEPROM (vorinstalliert) – unverändert

## Offene Punkte / Noch zu entscheiden
- [x] Name/Passwort des Ersteinrichtungs-AP (SSID "KORN-Setup", PW "Chaosfeeder")
- [x] NTP-Server (Standard: pool.ntp.org)
- [ ] Statische IP oder DHCP im Heimnetz?
- [x] DHCP-Hostname „KORN" (DHCP Option 12) – `WiFi.setHostname("KORN")` vor `WiFi.begin()`. Bei FritzBox/Speedport/OpenWRT direkt unter `http://korn` erreichbar
- [ ] mDNS (`http://korn.local`) – eigene Implementierung funktioniert auf Desktop (Firefox/Safari), aber **nicht zuverlässig auf Android** (Chrome/Firefox umgehen `.local` wegen DNS-over-HTTPS). Für Kundentauglichkeit verworfen. Alternative: DHCP-Hostname (siehe oben)
- [x] WLAN-Credentials nur im EEPROM (kein Fallback im Sketch)

## Zukünftige Verbesserungen (Nice-to-have)

### Web-UI / Mobile Experience
- [x] **Captive Portal**: DNS-Server auf Port 53 leitet alle Domains auf AP-IP (192.168.4.1). Android/iOS/Windows-Detection-URLs werden abgefangen → Browser öffnet sich automatisch nach WLAN-Verbindung
- [x] **Auto-Redirect**: Setup-Seite wird direkt unter `/` ausgeliefert im Einrichtungs-AP-Modus
- [x] **Improved Mobile UI**: Größere Touch-Targets (56px Buttons, 48px Inputs), bessere Touch-Feedback (aktive Zustände), optimierte Mobile-Styles (System-Fonts, Responsive Design)
- [x] **Hinweis über Motor-Ladezustand**: Info-Box zeigt an, dass Seite während Motorlauf eingefroren ist
- [x] **PWA-Support**: Web App Manifest (`/manifest.json`), Meta-Tags für iOS/Android, "Add to Home Screen" möglich, 🐔 Icon als SVG Data-URI
- [x] **Exit-Button auf Setup-Seite**: X-Button und "Abbrechen / Zurück" Button um zur Hauptseite zurückzukehren
- [x] **Factory Reset Button**: 🗑️ "Auf Werkseinstellungen zurücksetzen" unter NTP-Status, löscht EEPROM und startet neu (mit Confirm-Dialog)
- [ ] QR-Code Anzeige auf der Einrichtungsseite für schnelles WLAN-Sharing (optional via externem Generator)

## Status

### ✅ Abgeschlossen
- [x] Projektplan erstellt
- [x] Ordnerstruktur angelegt
- [x] wifi_client.ino (WLAN-Client + Ersteinrichtungs-AP)
- [x] main.ino (NTP statt DS1302, NTP-Resync alle 60min)
- [x] motor.ino (aus Rev3 übernommen, erweitert: Buzzer-Jagdsignal "Zum Essen")
- [x] homepage.ino (IP-Anzeige, NTP-Status, mobile UI)
- [x] README_REV3.5.md vollständig überarbeitet
- [x] WPA2-Hinweis dokumentiert (WPA3/Mixed-Mode Problematik)
- [x] Sicherheits-Abschnitt (Firewall, VLAN, HTTP-Risiken)
- [x] IP-Adresse finden ohne Admin-Rechte dokumentiert
- [x] Factory Reset Prozedur dokumentiert
- [x] Verborgene SSIDs (Hidden Networks) Unterstützung bestätigt

### 🔄 In Arbeit / Offen
- [ ] Hardware-Test mit echtem Setup (Verdrahtung + Motorlauf)
- [ ] Schaltplan überarbeiten (DS1302 entfernt, neue Pin-Belegung)
- [ ] NTP-Sync über Nacht testen (Zeitdrift, Sommer-/Winterzeit)
- [x] WLAN-Reconnect nach Router-Neustart testen (Logik: 3 Vordergrund-Versuche, dann 30s-Hintergrund-Loop, kein automatischer Setup-AP)

### 📝 Zukünftige Verbesserungen
- Siehe Abschnitt "Zukünftige Verbesserungen (Nice-to-have)"

### ⚠️ Sicherheits- und Stabilitäts-Verbesserungen (Rev3.5)
- [x] **Watchdog aktiviert** auf UNO R4 (4s Timeout via `WDT.h`)
- [x] **Watchdog-Refresh in Wait-Schleifen** (`connectToHome`, `ntpSync`, Recovery) – verhindert Boot-Loop
- [x] **EEPROM-Persistenz für Fütterungs-State** (verhindert Doppel-Fütterung nach Stromausfall)
- [x] **Setup-AP nur bei Erstinbetriebnahme** (vorher: bei jedem WLAN-Verlust)
- [x] **HTTP-Timeouts reduziert** (1500ms → 500ms, weniger Loop-Blockaden)
- [x] **HTML-Escaping & URL-Encoding** für Admin-Passwort (XSS-Schutz)
- [x] **Auth-Cache pro Request** (HTML/URL-Encoding nur einmal berechnet)
- [x] **String-Buffer reserviert** (512B upfront, weniger Heap-Fragmentation)
- [x] **Optionales Admin-Passwort** + Login-Seite
- [x] **Hardware-Recovery** (Pin 10↔11 5s halten beim Boot → Factory Reset)
- [x] **Serial-Wait mit Timeout** (Gerät startet auch ohne USB)
- [x] **DHCP-IP Wait** (verhindert IP=0.0.0.0 Logs)
- [x] **WPA2-Validierung** (8-63 Zeichen)
- [x] **Body-Size-Limit** (max. 2KB POST verhindert DoS)
- [x] **Laien-FAQ in README** (häufige Fragen für Hühnerfreunde)
