# K.O.R.N.
**Katastrophal Organisierter Nahrungsmittel Spender**

K.O.R.N. wurde geplant als robuster, Open-Source/Hardware, wasserdichter, mit einfachen Mitteln konstruierter und mäusesicherer Fütterungsautomat für Geflügel.

Aufgrund der enttäuschenden Erfahrung mit gekauften Fütterungsautomaten, welche trotz teils hoher Preise entweder nach drei Wochen defekt waren oder ganze Mäusefamilien durchfütterten, musste eine Eigenkonstruktion her. Die Entscheidungsgrundlage für das gewählte System mit einer Förderschnecke in einem Rohr basiert auf einer Recherche im Dubbel, Ausgabe von 2001.

Es handelt sich um einen Stetigförderer (Schnecke), der Schüttgut (Futter) aus einem Silo (KG-Rohr) in einen Auswurfschacht befördert. Die Fütterungszeit und die Menge sind im Arduino-Code einstellbar. Das Gehäuse besteht aus überall erhältlichen, robusten und günstigen HT- bzw. KG-Rohren, die das Futter vor Mäusen, Insekten und Feuchtigkeit schützen sollen.

Die elektronischen Bauteile sind leicht verfügbar, standardisiert und günstig zu beschaffen. Alle Datenblätter und Schaltpläne sind online auffindbar.

Die Bauteile der Förderschnecke wurden mit FreeCAD (LGPL2+, CC-BY-3.0) entworfen. Die Förderschnecke wurde steckbar entworfen, sodass sie auch auf kleineren 3D-Druckern gedruckt werden kann.

## Funktionsweise Rev2

Der Arduino UNO R4 WiFi steuert über eine **DS3231 Echtzeituhr (RTC)** eine tägliche Fütterungszeit. Das Datum der letzten Fütterung wird im flash-emulierten EEPROM des R4 WiFi gespeichert und überlebt so Stromausfälle und Resets. Die Fütterung erfolgt innerhalb eines definierbaren Fensters um die Zielzeit herum; danach gibt es ein Nachholfenster. Anschließend schaltet ein Relais die Stromversorgung des Motortreibers ein, der NEMA-17-Stepper fördert das Futter für eine einstellbare Dauer und wird danach wieder stromlos geschaltet.

## Materialliste

**Gekauft:**
- 1x NEMA-17 Stepper Motor (z. B. 17HS15-1504S-X1)
- 1x Arduino UNO R4 WiFi
- 1x DM320T Motortreiber
- 1x Relais-Modul (z. B. HW-482)
- 1x DS3231 RTC-Modul
- Diverse KG-/HT-Rohre
- Diverse Kabel und 5-mm-Stecker für Arduino
- 12 V / 1,5 A Netzteil
- 4x Schrauben M3 für Stepper
- Kleber für PLA (z. B. Pattex Ultra Gel Sekundenkleber)

**Selbst gedruckt:**
- 3x 3D-Druck Förderschnecke/Wendel
- 1x 3D-Druck Achsträger
- 1x 3D-Druck Motorträger
- 1x 3D-Druck Zahnrad-Achse
- 1x 3D-Druck Achse-Achsträger

## Verdrahtung

**DS3231 RTC:**
- VCC → Arduino 3,3 V oder 5 V
- GND → Arduino GND
- SDA → Arduino A4 (SDA)
- SCL → Arduino A5 (SCL)
- SQW → nicht anschließen! (Pin 2 ist durch PUL belegt; bei Bedarf z. B. Pin 7 nutzen)

**Motortreiber:**
- PUL+ → Arduino Pin 2
- DIR+ → Arduino Pin 3
- ENA+ → Arduino Pin 5

**Relais:**
- Relais IN → Arduino Pin 8

**EEPROM-Reset:**
- Jumper/Kabel zwischen Pin 12 und Pin 13 → löscht das gespeicherte Fütterungsdatum
- Beim Start: sofort löschen
- Im laufenden Betrieb: Jumper mindestens 3 Sekunden halten
- Nach dem Löschen Jumper entfernen (ansonsten wird bei jedem Neustart gelöscht)

## Konfiguration im Code

Die wichtigsten Parameter in `main/KORN-Motorsteuerung_Rev2.1_R4WiFi/KORN-Motorsteuerung_Rev2.1_R4WiFi.ino`:

> **Hinweis:** Die hier gezeigten Werte sind **Beispielwerte**. Alle Parameter (Fütterungszeit, Fütterungsdauer/Menge, Timing) werden direkt im Code an die eigenen Gegebenheiten angepasst (z. B. Größe der Hühnerschar) und können von diesem Beispiel abweichen.

```cpp
// Fütterungszeit (24h-Format)
// ACHTUNG: Keine führende Null verwenden (z.B. 07, 09) - C++ wertet das als Oktalzahl!
const int FUETTERUNG_STUNDE_1 = 7;   // Fütterung: 07:01 Uhr
const int FUETTERUNG_MINUTE_1 = 1;

// Motorparameter
const int MOTOR_BESCHLEUNIGUNG = 2000; // Beschleunigung in Steps/s²
const int SCHRITTFREQUENZ = 1000;      // Steps pro Sekunde (fix)
const int FUETTERUNG_DAUER_SEK = 5;    // Fütterungsdauer in Sekunden (1–60 s)
const bool MOTOR_RECHTS = false;        // true = rechts (CW), false = links (CCW)

// Timing-Parameter
const int RELAIS_VERZOEGERUNG = 2000;  // ms zwischen Relais-Ein und Motor-Start
const int MOTOR_PAUSE = 5000;          // ms nach Motorlauf bevor Relais aus
const int LOOP_INTERVALL = 10;         // Sekunden zwischen zwei Loop-Durchläufen
const int ZEIT_AUSGABE_INTERVALL = 10; // Statusausgabe alle 10 Sekunden
const int FUETTERUNGS_FENSTER_MIN = 2; // Fütterungsfenster: Zielzeit bis +2 Minuten
const int NACHHOLFENSTER_MIN = 60;    // Nachholfenster: Zielzeit bis +60 Minuten

// Automatische Zeitsynchronisation
const bool ZEIT_EINSTELLEN = false;    // Nur true, wenn lostPower() oder explizit gewünscht
```

**Hinweis zur Zeit:** Die RTC-Zeit wird automatisch auf die Build-Zeit gesetzt, wenn die RTC ihre Versorgung verloren hat (`lostPower()`) oder `ZEIT_EINSTELLEN` auf `true` gesetzt wird. Nach dem ersten Flashen ist eine manuelle Synchronisation nur bei Bedarf nötig.

**Serieller Monitor:** 9600 Baud einstellen.

## Fehlerverhalten / Sicherheit

- **RTC nicht gefunden:** Wird die DS3231 beim Start nicht erkannt, wird alle 5 Sekunden ein neuer Verbindungsversuch unternommen. Sobald die RTC wieder erreichbar ist, startet das System normal weiter.
- **Ungültige RTC-Zeit:** Liefert die RTC unplausible Werte (z. B. durch eine I2C-Störung), wird der Durchlauf übersprungen. Das verhindert falsche Tagesresets und Doppelfütterungen.
- **Watchdog:** Aktuell deaktiviert, da er einen Reset-Loop verursacht hat. Soll später über einen Hardware-Timer sauber wieder integriert werden.

## Fütterungslogik in einfachen Worten

Der Arduino merkt sich im EEPROM das **Datum der letzten Fütterung** (Tag, Monat, Jahr). Das reicht, weil sich das Datum durch Stromausfälle und Resets nicht verliert.

### Ablauf

1. **Beim Start** liest er dieses Datum aus dem EEPROM.
2. **Alle 10 Sekunden** liest er die aktuelle Uhrzeit und prüft die Fütterungsfenster.
3. Er berechnet die **Zielzeit** aus den konfigurierten Stunden/Minuten:
   - **Fütterungsfenster:** von Zielzeit bis Zielzeit + 2 Minuten
   - **Nachholfenster:** von Zielzeit + 2 Minuten bis Zielzeit + 60 Minuten
4. Wenn die aktuelle Uhrzeit in einem dieser Fenster liegt **und** an diesem Tag noch nicht gefüttert wurde, startet die Fütterung.
5. **Direkt vor dem Motorstart** wird das aktuelle Datum ins EEPROM geschrieben. Damit vermeidet der Arduino Doppelfütterungen, wenn der Strom während des Fensters mehrfach aus- und wieder eingeschaltet wird.
6. Nach dem Füttern läuft der Arduino weiter. Bis zum nächsten Tag passiert nichts mehr, weil das gespeicherte Datum jetzt mit dem aktuellen Tag übereinstimmt.

### Tageswechsel

Ein extra Reset um Mitternacht ist nicht nötig. Sobald der aktuelle Tag sich ändert, ist das gespeicherte Fütterungsdatum automatisch kleiner. Dadurch ist die Fütterung am nächsten Tag wieder freigegeben.

### Verhalten bei Stromausfall

| Situation | Verhalten |
|---|---|
| Strom weg **vor** der Fütterungszeit | Nach dem Einschalten wartet der Arduino, bis das Fütterungsfenster erreicht ist, und füttert dann. |
| Strom weg **im Fütterungsfenster** | Nach dem Einschalten füttert er sofort, weil das Datum noch nicht gespeichert war. |
| Strom weg **im Nachholfenster** | Nach dem Einschalten füttert er sofort nach. |
| Strom weg **während des Fütterns** | Das Datum wurde bereits gespeichert. Beim Neustart füttert er nicht erneut. |
| Strom weg **nach dem Füttern** | Das Datum ist gespeichert, es passiert bis zum nächsten Tag nichts mehr. |
| Strom weg **länger als 60 Minuten** | Es wird nicht nachgeholt. Die nächste Fütterung findet am nächsten Tag statt. |

> **Kompromiss:** Weil das Datum vor dem Motorstart gespeichert wird, kann ein Stromausfall während des eigentlichen Füttervorgangs nicht erkannt werden. Dafür wird das Risiko einer Doppelfütterung im Fütterungsfenster komplett vermieden.

## Software / Bibliotheken

Für den Arduino werden folgende Bibliotheken benötigt:
- [AccelStepper](https://www.airspayce.com/mikem/arduino/AccelStepper/)
- [RTClib](https://github.com/adafruit/RTClib)
- [Wire](https://www.arduino.cc/en/reference/wire) (Standardbibliothek)
- EEPROM (Standardbibliothek für Arduino UNO R4 WiFi)

## Code-Verlauf

1. **Setup:** Initialisierung von RTC, I2C, Ausgängen und Auslesen des letzten Fütterungsdatums aus dem EEPROM. Serielle Ausgabe erfolgt mit 9600 Baud. Optional: Jumper Pin 12↔13 löscht das Fütterungsdatum.
2. **Hauptschleife (alle 10 Sekunden):**
   - Plausibilitätsprüfung der RTC-Zeit.
   - Alle 30 Sekunden: Ausgabe von aktueller Zeit, Temperatur, letzter Fütterung und Zeit bis zur nächsten Fütterung.
   - Prüfung, ob das heutige oder gestrige Fütterungsfenster aktiv ist und an diesem Tag noch nicht gefüttert wurde.
   - Das Fütterungsdatum wird **vor** dem Motorstart ins EEPROM geschrieben, um Doppelfütterungen zu vermeiden.
   - Fütterungsvorgang: Relais ein → Motor aktivieren → Futter fördern → Motor deaktivieren → Relais aus.

## Ausblick

- Austausch des Arduino Uno gegen ein WLAN-fähiges Gerät.
- Anbindung an einen Telegram-Bot für Statusmeldungen.
- Manuelle Bedienelemente mit Display für Fütterungszeit, Menge und manuelle Abgabe.
- Förderschnecke in stabilerem Material als PLA drucken.

## Lizenz

K.O.R.N. wird als Open-Source-Projekt unter der Creative Commons Attribution-NonCommercial-ShareAlike (CC BY-NC-SA) veröffentlicht.
