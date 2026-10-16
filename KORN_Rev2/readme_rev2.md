# K.O.R.N.
**Katastrophal Organisierter Nahrungsmittel Spender**

K.O.R.N. wurde geplant als robuster, Open-Source/Hardware, wasserdichter, mit einfachen Mitteln konstruierter und mäusesicherer Fütterungsautomat für Geflügel.

Aufgrund der enttäuschenden Erfahrung mit gekauften Fütterungsautomaten, welche trotz teils hoher Preise entweder nach drei Wochen defekt waren oder ganze Mäusefamilien durchfütterten, musste eine Eigenkonstruktion her. Die Entscheidungsgrundlage für das gewählte System mit einer Förderschnecke in einem Rohr basiert auf einer Recherche im Dubbel, Ausgabe von 2001.

Es handelt sich um einen Stetigförderer (Schnecke), der Schüttgut (Futter) aus einem Silo (KG-Rohr) in einen Auswurfschacht befördert. Die Fütterungszeit und die Menge sind im Arduino-Code einstellbar. Das Gehäuse besteht aus überall erhältlichen, robusten und günstigen HT- bzw. KG-Rohren, die das Futter vor Mäusen, Insekten und Feuchtigkeit schützen sollen.

Die elektronischen Bauteile sind leicht verfügbar, standardisiert und günstig zu beschaffen. Alle Datenblätter und Schaltpläne sind online auffindbar.

Die Bauteile der Förderschnecke wurden mit FreeCAD (LGPL2+, CC-BY-3.0) entworfen. Die Förderschnecke wurde steckbar entworfen, sodass sie auch auf kleineren 3D-Druckern gedruckt werden kann.

## Funktionsweise Rev2

Der Arduino steuert über eine **DS3231 Echtzeituhr (RTC)** eine tägliche Fütterungszeit. Vor der Fütterung gibt der aktive Buzzer eine Warnung aus. Anschließend schaltet ein Relais die Stromversorgung des Motortreibers ein, der NEMA-17-Stepper fördert das Futter für eine einstellbare Dauer und wird danach wieder stromlos geschaltet.

## Materialliste

**Gekauft:**
- 1x NEMA-17 Stepper Motor (z. B. 17HS15-1504S-X1)
- 1x Arduino Uno
- 1x DM320T Motortreiber
- 1x Relais-Modul (z. B. HW-482)
- 1x DS3231 RTC-Modul
- 1x aktiver Buzzer (5 V, HIGH = Ton)
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

**Relais & Buzzer:**
- Relais IN → Arduino Pin 8
- Buzzer SIG → Arduino Pin 6 (aktiver Buzzer, HIGH = Ton)

## Konfiguration im Code

Die wichtigsten Parameter in `main/KORN-Motorsteuerung_Rev2/KORN-Motorsteuerung_Rev2.ino`:

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
const int ZEIT_AUSGABE_INTERVALL = 30; // Zeitausgabe alle 30 Sekunden

// Sound-Parameter
const int BUZZER_VORWARNUNG = 3;       // Wartezeit in Sekunden zwischen Warnung und Fütterung
const int BUZZER_ANZAHL_TOENE = 3;     // Anzahl Warntöne vor Fütterung
const int BUZZER_TON_DAUER = 1000;     // ms Dauer pro Ton
const int BUZZER_PAUSE_DAUER = 200;    // ms Pause zwischen Tönen

// Automatische Zeitsynchronisation
const bool ZEIT_EINSTELLEN = false;    // Nur true, wenn lostPower() oder explizit gewünscht
```

**Hinweis zur Zeit:** Die RTC-Zeit wird automatisch auf die Build-Zeit gesetzt, wenn die RTC ihre Versorgung verloren hat (`lostPower()`) oder `ZEIT_EINSTELLEN` auf `true` gesetzt wird. Nach dem ersten Flashen ist eine manuelle Synchronisation nur bei Bedarf nötig.

## Fehlerverhalten / Sicherheit

- **RTC nicht gefunden:** Wird die DS3231 beim Start nicht erkannt, gibt der Buzzer alle 5 Sekunden einen kurzen Warnton aus und versucht laufend, die Verbindung wiederherzustellen. Sobald die RTC wieder erreichbar ist, startet das System normal weiter.
- **Ungültige RTC-Zeit:** Liefert die RTC unplausible Werte (z. B. durch eine I2C-Störung), wird der Durchlauf übersprungen. Das verhindert falsche Tagesresets und Doppelfütterungen.
- **Watchdog:** Ein 2-Sekunden-Watchdog startet den Arduino automatisch neu, falls das Programm hängen bleibt.

## Software / Bibliotheken

Für den Arduino werden folgende Bibliotheken benötigt:
- [AccelStepper](https://www.airspayce.com/mikem/arduino/AccelStepper/)
- [RTClib](https://github.com/adafruit/RTClib)
- [Wire](https://www.arduino.cc/en/reference/wire) (Standardbibliothek)

## Code-Verlauf

1. **Setup:** Initialisierung von RTC, I2C, Watchdog und Ausgängen.
2. **Powerup-Signal:** Zwei Buzzertöne signalisieren den Start.
3. **Hauptschleife (alle 30 Sekunden):**
   - Plausibilitätsprüfung der RTC-Zeit.
   - Aktuelle Zeit und Temperatur per Seriell ausgeben.
   - Täglicher Reset des Fütterungsflags um Mitternacht.
   - Prüfung der konfigurierten Fütterungszeit.
   - Fütterungsvorgang: Warnung → Relais ein → Motor aktivieren → Futter fördern → Motor deaktivieren → Relais aus.

## Ausblick

- Austausch des Arduino Uno gegen ein WLAN-fähiges Gerät.
- Anbindung an einen Telegram-Bot für Statusmeldungen.
- Manuelle Bedienelemente mit Display für Fütterungszeit, Menge und manuelle Abgabe.
- Förderschnecke in stabilerem Material als PLA drucken.

## Lizenz

K.O.R.N. wird als Open-Source-Projekt unter der Creative Commons Attribution-NonCommercial-ShareAlike (CC BY-NC-SA) veröffentlicht.
