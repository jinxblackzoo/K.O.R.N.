# K.O.R.N.
**Katastrophal Organisierter Runder Nahrungsmittelspender**

K.O.R.N. ist ein robuster, Open-Source/Hardware, wasserdichter, mit einfachen Mitteln konstruierter und mäusesicherer Fütterungsautomat für Geflügel.

## Lizenz
Attribution-NonCommercial-ShareAlike 4.0 International

## 🎯 Projektübersicht

Aufgrund der enttäuschenden Erfahrung mit gekauften Fütterungsautomaten welche trotz der teils hohen Preise entweder nach drei Wochen defekt waren, oder ganze Mäusefamilien durchfütterten, musste eine Eigenkonstruktion her. Die Entscheidungsgrundlage für das gewählte System mit einer Förderschnecke in einem Rohr, basiert auf einer Recherche im Dubbel Ausgabe von 2001.

Es handelt sich um einen **Stetigförderer (Schnecke)**, der Schüttgut (Futter) aus einem Silo (KG-Rohr) in einen Auswurfschacht befördert. Das Gehäuse besteht aus überall erhältlichen, robusten und günstigen HT-, bzw KG-Rohren.

## Revision 1 wurde verworfen

## Revision 2 enthält die neuen CAD-Dateien und eine Steuerung mit ARDUINO Uno R3

Revision 2 wurde entwickelt als supersimple, autarke und stromsparende Version ohne Extra-Features.

## Revision 3 basiert auf den CAD Dateien von Rev.2 und einer Steuerung mit ARDUINO Uno R4 WiFi ✅ Fertig

Revision 3 wurde entwickelt als autarke Off-Grid Funktion. Ein WLAN mit Internetverbindung ist hier nicht vorgesehen.
Stattdessen kann KORN mitten im Feld ohne eigene Internetverbindung installiert werden. Der Arduino UNO R4 spannt einen eigenen WLAN Accesspoint auf. Mit diesem kann man sich mittels Smartphone verbinden.
Tipp: Alte Smartphones ohne Internet/Benutzerdaten einfach auf Werkseinstellungen zurücksetzen. Dann mit dem WLAN "KORN" verbinden und im Browser `192.168.4.1` öffnen. Voilà, fertig ist das eigene CCCC (ChickenCoopControlCenter) 🥳 😉

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

**Ablauf:** Beim ersten Start oder bei fehlendem WLAN öffnet der Arduino einen temporären Einrichtungs-AP. Nach Speichern der Zugangsdaten verbindet sich KORN automatisch mit dem Heimnetz.

📖 **Detaillierte Dokumentation:** [`KORN_Rev3.5/README_REV3.5.md`](./KORN_Rev3.5/README_REV3.5.md)

## Revision 4 basiert auf Rev.3/3.5 CAD mit neuem Controller: Raspberry Pi Pico W (Geplant)

Revision 4 übernimmt die überarbeiteten CAD-Dateien (Wendel 4.0, neuer Achsträger, Arduino-Träger als Pico-Träger).
Als Controller wird ein Raspberry Pi Pico W eingesetzt. Die Software wird in MicroPython neu geschrieben.
Ebenfalls geplant ist eine eigene Platine für eine sauberere Verkabelung und Befestigung.




