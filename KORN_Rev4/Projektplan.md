# K.O.R.N. Rev4 – Projektplan

## Hardware-Änderungen
- **Controller-Wechsel**: Arduino UNO R4 WiFi → Raspberry Pi Pico W
- **RTC DS1302 entfällt**: Zeitsteuerung über NTP (Internetzeit aus Heimnetz)
- **EEPROM entfällt**: Konfigurationsspeicherung im Pico W Flash (JSON-Datei)
- Stepper-Treiber DM320T, Relais, Buzzer, Taster bleiben unverändert
- Pin-Belegung wird neu festgelegt (noch nicht verdrahtet)
- Erstellen einer Platine um eine bessere Befestigung und Verkabelung zu ermöglichen

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
