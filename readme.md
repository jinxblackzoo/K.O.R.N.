# K.O.R.N.
**Katastrophal Organisierter Runder Nahrungsmittelspender**

K.O.R.N. ist ein robuster, Open-Source/Hardware, wasserdichter, mit einfachen Mitteln konstruierter und mäusesicherer Fütterungsautomat für Geflügel.

## Lizenz
Attribution-NonCommercial-ShareAlike 4.0 International

## 🎯 Projektübersicht

Aufgrund der enttäuschenden Erfahrung mit gekauften Fütterungsautomaten welche trotz der teils hohen Preise entweder nach drei Wochen defekt waren, oder ganze Mäusefamilien durchfütterten, musste eine Eigenkonstruktion her. Die Entscheidungsgrundlage für das gewählte System mit einer Förderschnecke in einem Rohr, basiert auf einer Recherche im Dubbel Ausgabe von 2001.

Es handelt sich um einen **Stetigförderer (Schnecke)**, der Schüttgut (Futter) aus einem Silo (KG-Rohr) in einen Auswurfschacht befördert. Das Gehäuse besteht aus überall erhältlichen, robusten und günstigen HT-, bzw KG-Rohren.

## 📊 Revisions-Übersicht

| Version | Board | Zeitquelle | Netzwerk | Bedienung | Wesentliche Änderungen |
|---|---|---|---|---|---|
| **Rev2** | Arduino Uno | DS3231 (I2C) | – | keine (Zeiten im Code) | Urversion: 1 Fütterungszeit, Buzzer-Warnung, Relais + Stepper |
| **Rev2.1** | UNO R4 WiFi | DS3231 (I2C) | – (WiFi ungenutzt) | keine | Port auf R4 WiFi; Fütterungsdatum im flash-EEPROM; Reset per Jumper Pin 12↔13; RTC-Boot-Timeout + Motor-Timeout |
| **Rev3** | UNO R4 WiFi | DS1302 (D7/D9/D12) | eigener AP „KORN" | Web-UI | Modularer Sketch; Web-UI mit 2 Zeiten, Laufzeit, Sofortfütterung, Blockadelöser; Config in DS1302-RAM + EEPROM-Backup |
| **Rev3.1** | UNO R4 WiFi | DS3231 (I2C) | eigener AP „KORN" | Web-UI | RTC-Tausch → D7/D9/D12 frei; Config nur EEPROM; Jagdsignal; Stabilitäts-Paket aus 2.1: Nachholfenster, persistenter Fütterungszustand, Watchdog, I2C-Timeout, Event-Log auf der Startseite |
| **Rev3.5** | UNO R4 WiFi | NTP (Heimnetz) | WLAN-Client; Setup-AP „KORN-Setup" | Web-UI im Heimnetz | RTC entfällt; WLAN-Daten im EEPROM; Event-Log `/log`; Factory Reset; Watchdog; Sommer-/Winterzeit; Rev4-CAD enthalten |
| **Rev3.6** | UNO R4 WiFi | NTP + Intervall | wie 3.5 + Fallback-AP | wie 3.5 | Notfütterung 8/16 h à 10 s ohne NTP; Fallback-AP bei Heimnetz-Verlust; ⚠️ **unfertig – kompiliert nicht** |
| **Rev4** | (geplant: Pico W) | – | – | – | Nur CAD: Mechanik-Redesign (Wendel 4.0, Dichtring, Controller-Träger); Software in MicroPython geplant |

**Entwicklungslinien:**
- **Off-Grid** (eigene RTC + eigenes WLAN): Rev2 → Rev2.1 → Rev3 → **Rev3.1**
- **Heimnetz** (NTP + WLAN-Client): Rev3.5 → Rev3.6 (unfertig)
- **Rev4:** Mechanik-Redesign mit neuem Controller (geplant)

## Revision 1 wurde verworfen

## Revision 2 enthält die neuen CAD-Dateien und eine Steuerung mit ARDUINO Uno R3

Revision 2 wurde entwickelt als supersimple, autarke und stromsparende Version ohne Extra-Features.

## Revision 3 basiert auf den CAD Dateien von Rev.2 und einer Steuerung mit ARDUINO Uno R4 WiFi ✅ Fertig

Revision 3 wurde entwickelt als autarke Off-Grid Funktion. Ein WLAN mit Internetverbindung ist hier nicht vorgesehen.
Stattdessen kann KORN mitten im Feld ohne eigene Internetverbindung installiert werden. Der Arduino UNO R4 spannt einen eigenen WLAN Accesspoint auf. Mit diesem kann man sich mittels Smartphone verbinden.
Tipp: Alte Smartphones ohne Internet/Benutzerdaten einfach auf Werkseinstellungen zurücksetzen. Dann mit dem WLAN "KORN" verbinden und im Browser `192.168.4.1` öffnen. Voilà, fertig ist das eigene CCCC (ChickenCoopControlCenter) 🥳 😉

## Revision 3.1 basiert auf Rev.3 mit RTC DS3231 statt DS1302 ✅ Fertig

Revision 3.1 ist funktional identisch mit Rev.3 (Off-Grid Access Point), verwendet jedoch die **DS3231 RTC** (I2C, SDA=A4, SCL=A5) anstelle der DS1302. Da die DS3231 kein nutzbares RAM besitzt, wird die Konfiguration ausschließlich im EEPROM des Arduino gespeichert. Als Bibliothek kommt **RTClib by Adafruit** statt "Rtc by Makuna" zum Einsatz.

**Stabilitäts-Paket** (übernommen aus der 3 Monate lang bewährten Rev2.1):
- **Nachhol-Fenster**: Verpasste Fütterung (Strom-/Solar-Ausfall zur Zielzeit) wird bis zu 60 min nachgeholt
- **Persistenter Fütterungszustand** im EEPROM → keine Doppelfütterung nach Reboot
- **Hardware-Watchdog** (4 s, WDT.h auf UNO R4) mit Refresh in Loop + Motorlauf
- **I2C-Timeout** (3 s) + erweiterte RTC-Plausibilitätsprüfung (alle Datumsfelder) + Build-Zeit-Sanity
- **Event-Log**: letzte 10 Fehler/Ereignisse direkt auf der Web-Startseite

📖 **Detaillierte Dokumentation:** [`KORN_Rev3.1/readme_rev3.1.md`](./KORN_Rev3.1/readme_rev3.1.md)

## Revision 3.5 basiert auf Rev.3 (Arduino UNO R4 WiFi) mit neuer WLAN-Logik ✅ Fertig

Revision 3.5 verwendet die gleiche Hardware und CAD wie Rev.3/Rev.4, ändert jedoch den WLAN-Betrieb grundlegend:
Statt eines eigenen Access Points verbindet sich KORN nun mit einem vorhandenen Heimnetz (WLAN-Router).

**Neue Features:**
- **DHCP-Hostname "KORN"**: Im Router als Gerät "KORN" sichtbar, direkt erreichbar unter `http://korn` (FritzBox, Speedport, OpenWRT)
- **Captive Portal**: Automatische Browser-Öffnung bei Einrichtungs-AP (`KORN-Setup`, PW: `Chaosfeeder`)
- **NTP-Zeitsteuerung**: Internetzeit statt DS1302 RTC (mit Sommer-/Winterzeit)
- **Buzzer-Jagdsignal**: Akustisches Signal "Zum Essen" (3/8-Takt) während der Fütterung
- **WPA2-Unterstützung** mit Passwort-Validierung (8-63 Zeichen)
- **Optionales Admin-Passwort**: Web-UI-Schutz vor unautorisierten Zugriffen im Heimnetz
- **Hardware-Recovery**: Bei vergessenem Passwort – Knopf (Pin 10↔11) zur Laufzeit 10s halten → EEPROM-Reset (Buzzer warnt ab 5s, Abbruch durch Loslassen möglich)

**24/7-Betriebssicherheit:**
- **Hardware-Watchdog (4s)**: Automatischer Reboot bei Code-Hängern
- **Stromausfall-Schutz**: EEPROM persistiert letzte Fütterung → keine Doppel-Fütterung nach Reboot
- **Robuster WLAN-Reconnect**: Bei vorhandenen Credentials kein automatischer Setup-AP-Modus
- **HTTP-Timeouts optimiert**: 500ms statt 1500ms → weniger Loop-Blockaden
- **XSS-Schutz**: HTML-Escaping & URL-Encoding für Admin-Passwort
- **NTP-Drift-Schutz**: Erkennt kleine Rückwärtssprünge bei NTP-Resync und verhindert dadurch ausgelöste Geister-Fütterungen (Mai 2026)

**Diagnose & Monitoring:**
- **Volles Datum + Uhrzeit** (`YYYY-MM-DD HH:MM:SS`) auf Web-UI und seriellem Monitor
- **Event-Log auf der Webseite** (`/log`): RAM-Ringpuffer mit den letzten 24 Scheduler-Ereignissen (Boot, NTP-Sync, Crossings, Trigger, Tageswechsel, Drift) – Fehlersuche ohne angeschlossenen Laptop

**Ablauf:** Beim ersten Start oder bei fehlendem WLAN öffnet der Arduino einen temporären Einrichtungs-AP. Nach Speichern der Zugangsdaten verbindet sich KORN automatisch mit dem Heimnetz.

📖 **Detaillierte Dokumentation:** [`KORN_Rev3.5/readme_rev3.5.md`](./KORN_Rev3.5/readme_rev3.5.md)

## Revision 4 basiert auf Rev.3/3.5 CAD mit neuem Controller: Raspberry Pi Pico W (Geplant)

Revision 4 übernimmt die überarbeiteten CAD-Dateien (Wendel 4.0, neuer Achsträger, Arduino-Träger als Pico-Träger).
Als Controller wird ein Raspberry Pi Pico W eingesetzt. Die Software wird in MicroPython neu geschrieben.
Ebenfalls geplant ist eine eigene Platine für eine sauberere Verkabelung und Befestigung.




