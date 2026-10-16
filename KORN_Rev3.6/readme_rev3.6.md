# K.O.R.N. Rev3.6

**Katastrophaler organisierter runder Nahrungsmittelspender**

Rev3.6 baut auf Rev3.5 auf: ein Arduino UNO R4 WiFi steuert die Fütterung, holt die Zeit per NTP und bietet eine Web-UI. Die wichtigste Neuerung gegenüber Rev3.5 ist ein **Notfütterungs-Modus**: auch ohne WLAN oder Internet füttert KORN in einem 8/16h-Rhythmus, bis wieder NTP synchronisiert ist.

## Hardware

- **Arduino UNO R4 WiFi**
- **NEMA-17 Stepper Motor**
- **DM320T Stepper-Motor Treiber**
- **Relais-Modul HW-482** oder ähnlich (Stromversorgung Motor ein/aus)
- **Aktiver Buzzer** (5 V, HIGH = Ton)
- **Taster**: Pin 10 ↔ Pin 11 (Kurzschluss = Fütterung, 10 s = Factory Reset)

## Verkabelung

```
Stepper-Treiber DM320T (Steuersignale):
├── PUL+    → Arduino Pin 2 (Schrittimpulse)
├── DIR+    → Arduino Pin 3 (Drehrichtung)
├── ENA+    → Arduino Pin 5 (Motor aktivieren)
├── OPTO    → Arduino 5V (gemeinsamer Plus der Optokoppler)
└── GND     → Arduino GND

Stepper-Treiber DM320T (Leistung):
├── GND     → 12V Netzteil -
├── +Vdc    → Relais NO
├── A+/A-   → NEMA17 Phase A
└── B+/B-   → NEMA17 Phase B

Buzzer:
├── VCC     → Arduino 5V
├── GND     → Arduino GND
└── SIG     → Arduino Pin 6

Relais:
├── S       → Arduino Pin 8
├── +       → Arduino 5V
└── -       → Arduino GND

Relais Schaltkontakte:
├── NO      → DM320T +Vdc
├── COM     → 12V Netzteil + (Mittlerer Pin)
└── NC      → nicht belegt

Taster:
└── Pin 10 ↔ Pin 11 (Kurzschluss = Fütterung)
```

### Hinweise zur Verkabelung

- `OPTO` ist **kein Arduino-Pin**, sondern die gemeinsame +5V-Versorgung der Optokoppler im DM320T.
- `PUL-`, `DIR-`, `ENA-` des DM320T an Arduino GND.
- Gemeinsame Masse zwischen Arduino und 12V-Netzteil sicherstellen.
- Relais trennt nur die Plusleitung: 12V+ → COM → NO → +Vdc des DM320T.
- `RELAY_ACTIVE_HIGH` in `motor.ino` anpassen, falls dein Relais-Modul LOW-aktiv ist.

## Software-Features

- **Zwei Fütterungszeiten** (24h-Format), zweite Zeit optional deaktivierbar
- **NTP-Zeitsteuerung** mit automatischer Sommer-/Winterzeit (Europa/Berlin)
- **NTP-Resync alle 60 Minuten** (im Heimnetz) bzw. alle 12 Stunden (Fallback-AP)
- **Notfütterung ohne WLAN**: 8/16h-Rhythmus, 10 s pro Lauf, bis NTP wieder synchronisiert
- **Web-UI**: Einstellung der Zeiten, Sofortfütterung, Status, Event-Log
- **Event-Log** (`/log`) im RAM-Ringpuffer mit den letzten 24 Ereignissen
- **Factory Reset**: Pin 10 ↔ 11 für 10 s halten (Buzzer warnt ab 5 s)
- **Hardware-Watchdog** (4 s)
- **Mindestabstand 2 Minuten** zwischen Fütterungen
- **EEPROM-Speicherung** für WLAN-Zugangsdaten und Konfiguration

## Konfiguration im Code

In `Script_KORN_REV3.6/main/main.ino`:

```cpp
const int FUETTERUNG_STUNDE_1  = 7;   // Erste Fütterung: 07:01 Uhr
const int FUETTERUNG_MINUTE_1  = 1;
const int FUETTERUNG_STUNDE_2  = 16;  // Zweite Fütterung: 16:01 Uhr
const int FUETTERUNG_MINUTE_2  = 1;
bool ZWEITE_ZEIT_AKTIV = true;        // false = zweite Fütterung deaktiviert

const int FEED_STEPS_PER_SEC = 1000;  // feste Schrittfrequenz
const int MOTOR_SCHRITTE = 2000;      // legacy, wird aus Fütterungsdauer berechnet
const bool MOTOR_DIR_CW = false;      // false = CCW, true = CW
```

In `Script_KORN_REV3.6/main/wifi_client.ino`:

```cpp
static const char* SETUP_AP_SSID = "KORN-Setup";
static const char* SETUP_AP_PASS = "12345678";   // mindestens 8 Zeichen!
```

## Ersteinrichtung

1. Sketch auf Arduino UNO R4 WiFi flashen.
2. Arduino startet. Wenn noch keine WLAN-Zugangsdaten im EEPROM gespeichert sind, öffnet er den Einrichtungs-AP `KORN-Setup`.
3. Mit Smartphone/PC mit `KORN-Setup` verbinden (Passwort siehe Code, Standard `12345678`).
4. Browser öffnet sich automatisch (Captive Portal) oder manuell `http://192.168.4.1/setup` aufrufen.
5. Heimnetz-SSID und -Passwort eingeben, optional Admin-Passwort für die Web-UI.
6. Speichern → Arduino startet neu und verbindet sich mit dem Heimnetz.
7. IP-Adresse wird im seriellen Monitor ausgegeben; Web-UI unter dieser IP erreichbar.

## Betrieb ohne WLAN (Notfütterung)

Falls kein WLAN verfügbar ist, kann KORN im Notfütterungs-Modus laufen:

- Der Arduino spannt einen Fallback-AP `KORN-Setup` auf.
- Alternativ kann die Uhrzeit nicht per NTP geholt werden → KORN füttert im 8/16h-Rhythmus:
  - erste Notfütterung 8 h nach Boot
  - dann abwechselnd 16 h / 8 h
  - jede Notfütterung läuft 10 s
- Sobald NTP einmal synchronisiert ist, übernimmt der normale Zeitplan.

## Web-UI

- Zwei Fütterungszeiten einstellen
- Zweite Zeit aktivieren/deaktivieren
- Motor-Laufzeit in Sekunden festlegen
- „Jetzt füttern“-Button
- Status: Uhrzeit, NTP-Status, letzte Fütterung, nächste Fütterung
- Event-Log unter `/log`

## Projektstruktur

```
KORN_Rev3.6/
├── readme_rev3.6.md
├── Script_KORN_REV3.6/
│   └── main/
│       ├── main.ino        (Setup, Loop, Scheduler, NTP, Konfiguration)
│       ├── motor.ino       (Stepper, Relais, Buzzer)
│       ├── wifi_client.ino (WLAN-Client, Einrichtungs-AP, Captive Portal)
│       └── homepage.ino    (Webserver, Web-UI, Event-Log)
```

## Benötigte Bibliotheken

- Arduino UNO R4 Boards (Boards Manager)
- AccelStepper (Library Manager)
- WiFiS3 (vorinstalliert)
- WiFiUdp (vorinstalliert)
- EEPROM (vorinstalliert)
- WDT.h / avr/wdt.h (vorinstalliert, plattformabhängig)

## Status

In Entwicklung / Test. Rev3.6 ist der aktuelle WLAN-Client-Branch mit Notfütterung.
