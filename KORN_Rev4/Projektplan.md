# K.O.R.N. Rev4 – Projektplan

## Hardware-Änderungen
- **Controller-Wechsel**: Arduino UNO R4 WiFi → Raspberry Pi Pico W
- **RTC DS1302 entfällt**: Zeitsteuerung über NTP (Internetzeit aus Heimnetz)
- **EEPROM entfällt**: Konfigurationsspeicherung im Pico W Flash (JSON-Datei)
- Relais, Buzzer, Taster bleiben unverändert
- Pin-Belegung wird neu festgelegt (noch nicht verdrahtet)
- Erstellen einer Platine um eine bessere Befestigung und Verkabelung zu ermöglichen

## 💡 Designentscheidungen für die eigene Platine (offen)

### Stepper-Treiber: TMC2209 statt DM320T?
**TMC2209** ist deutlich günstiger (~5 € vs. ~25 €) und bringt einige Vorteile mit:

| Vorteil | Bedeutung für KORN |
|---------|---------------------|
| **Silent-Stepping** | Kein lautes Klacken mehr – Hühner werden weniger gestresst |
| **StallGuard eingebaut** | Blockadeerkennung ohne extra Sensor |
| **CoolStep** | Reduziert Stromaufnahme automatisch bei niedriger Last |
| **Kompakt** | Passt direkt aufs Custom-PCB (Stepstick-Format) |
| **Preis** | ~20 € günstiger pro Gerät |

**Nachteile / zu beachten:**
- Max. 1.4 A RMS dauerhaft (DM320T: 2.2 A RMS) → bei größeren Motoren grenzwertig. Für aktuell verwendeten NEMA17 mit aktiver Kühlung (kleiner Lüfter oder Kühlkörper) OK.
- Max. 29 V Eingang (DM320T: 50 V) → bei 12 V Betrieb von KORN egal.
- Empfindlicher gegen Back-EMF (lockere Motorstecker) → fest verschraubte Klemmen vorsehen.
- Outdoor-tauglich nur in geschlossenem Gehäuse → ist im Hühnerstall ohnehin Pflicht.
- StallGuard braucht UART-Verbindung (1 GPIO + 1 kΩ-Widerstand) und einmaliges Sensitivity-Tuning.

**Entscheidung:** TBD – Kandidat für die Custom-PCB (siehe Blockadeerkennung).

### Blockadeerkennung der Förderschnecke
Bisher (Rev3.5) gibt es nur den manuell auslösbaren „Blockade lösen"-Button. Eine **automatische** Erkennung wäre wünschenswert (z.B. zum automatischen Rechtslauf-Versuch oder Alarm via Web-UI).

**Optionen, sortiert nach Aufwand:**

| # | Variante | Hardware-Aufwand | Kosten | Zuverlässigkeit | Bemerkung |
|---|----------|-------------------|--------|------------------|-----------|
| 1 | **TMC2209 StallGuard** | Treiberwechsel + UART-GPIO | ~5 € (statt DM320T) | hoch (nach Tuning) | Eleganteste Lösung, wenn ohnehin TMC2209 |
| 2 | **Hall-Sensor + Magnet an Schneckenwelle** | 1 GPIO + 5V + GND, Magnet einkleben/eindrucken | ~3 € | sehr hoch | Funktioniert mit beliebigem Treiber, robust gegen Staub/Feuchte |
| 3 | **Optischer Encoder** | 2 GPIO (Quadratur) | ~5–10 € | hoch | Empfindlich gegen Staub/Schmutz im Stall |
| 4 | **INA219 Stromsensor (I²C)** | 2 GPIO (I²C) | ~4 € | gering bei Constant-Current-Treibern | Bei DM320T fast nutzlos, bei TMC2209 begrenzt |

**Empfehlung:** Wenn TMC2209 → **Variante 1 (StallGuard)** als primäre Erkennung.  
Falls DM320T behalten wird → **Variante 2 (Hall-Sensor)** als günstige, sehr robuste Lösung. Magnet kann beim 3D-Druck der Schnecke direkt mit eingebettet werden.

**Software-Hooks (egal welche Variante):**
- Bei Blockade-Erkennung: Motor stoppen, Buzzer-Alarm, Fehler im Web-UI anzeigen
- Optional: 1-2× automatischer Rechtslauf-Versuch zum Lösen
- Logging der Blockade-Events (Zeitpunkt, Dauer) für Diagnose

## Software-Änderungen
- **Sprache**: Arduino C++ → MicroPython
- **WLAN-Modus**: Eigener Access Point (AP) → Client im Heimnetz (WLAN-Router)
- **Ersteinrichtungs-AP**: Falls keine WLAN-Zugangsdaten gespeichert oder WLAN nicht erreichbar → Pico W öffnet temporären AP zur Konfiguration per Handy
- **Zeitquelle**: DS1302 RTC → NTP-Sync aus dem Internet (Zeitzone Europe/Berlin, inkl. Sommer-/Winterzeit)
- **Persistenz**: EEPROM + DS1302-RAM → JSON-Datei im Pico W Flash

## Was bleibt gleich
- 2 Fütterungszeiten täglich, konfigurierbar
- Laufzeit 1–600 Sekunden
- 2-Minuten-Mindestabstand zwischen Fütterungen
- "Jetzt füttern"-Button (Web)
- "Blockade lösen (Rechtslauf)"-Button (Web)
- Manueller Hardware-Taster (GPIO-Kurzschluss)
- Mobilfreundliche deutschsprachige Web-UI
- Tages-Reset der Fütterungsmarker um Mitternacht
- Buzzer-Signal bei Fütterung

## Geplante Dateistruktur
```
KORN_Rev4/
├── Projektplan.md
└── Script_KORN_REV4/
    └── main/
        ├── main.py          – Hauptschleife, Orchestrierung
        ├── config.py        – Pin-Belegung, Defaults, Konstanten
        ├── wifi_manager.py  – WLAN-Client + Ersteinrichtungs-AP-Fallback
        ├── ntp_time.py      – NTP-Sync, Zeitzone, Fallback-Uhr
        ├── motor.py         – Stepper, Relais, Buzzer
        ├── scheduler.py     – Fütterungslogik, Gap-Schutz, Tagesreset
        ├── storage.py       – Konfiguration lesen/schreiben (JSON/Flash)
        ├── webserver.py     – HTTP-Routen (uasyncio)
        └── homepage.py      – Web-UI HTML-Generator
```

## Offene Punkte / Noch zu entscheiden
- [ ] Konkrete GPIO-Pinnummern festlegen (nach Pinout-Diagramm)
- [ ] **Stepper-Treiber wählen**: TMC2209 (silent + StallGuard) oder DM320T behalten?
- [ ] **Blockadeerkennung**: StallGuard (TMC) oder Hall-Sensor (universell)?
- [ ] NTP-Server (Standard: pool.ntp.org)
- [ ] Name/Passwort des Ersteinrichtungs-AP
- [ ] Hostname im Heimnetz (z.B. http://korn.local via mDNS)
- [ ] Mehr als 2 Fütterungszeiten gewünscht? (technisch einfach erweiterbar)

## Status
- [ ] Projektplan erstellt
- [ ] Dateistruktur anlegen
- [ ] config.py
- [ ] storage.py
- [ ] wifi_manager.py
- [ ] ntp_time.py
- [ ] motor.py
- [ ] scheduler.py
- [ ] webserver.py
- [ ] homepage.py
- [ ] main.py
- [ ] README Rev4 aktualisieren
