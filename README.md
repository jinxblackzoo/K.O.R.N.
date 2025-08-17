# K.O.R.N.
**Katastrophal Organisierter Runder Nahrungsmittelspender**

K.O.R.N. ist ein robuster, Open-Source/Hardware, wasserdichter, mit einfachen Mitteln konstruierter und mäusesicherer Fütterungsautomat für Geflügel.

## 🎯 Projektübersicht

Aufgrund der enttäuschenden Erfahrung mit gekauften Fütterungsautomaten welche trotz der teils hohen Preise entweder nach drei Wochen defekt waren, oder ganze Mäusefamilien durchfütterten, musste eine Eigenkonstruktion her. Die Entscheidungsgrundlage für das gewählte System mit einer Förderschnecke in einem Rohr, basiert auf einer Recherche im Dubbel Ausgabe von 2001.

Es handelt sich um einen **Stetigförderer (Schnecke)**, der Schüttgut (Futter) aus einem Silo (KG-Rohr) in einen Auswurfschacht befördert. Das Gehäuse besteht aus überall erhältlichen, robusten und günstigen HT-, bzw KG-Rohren.

## Revision 1 wurde verworfen

## Revision 2 enthält die neuen CAD Dateien und eine Steuerung mit ARDUINO Uno R3

## Revision 3 basiert auf den CAD Dateien von Rev.2 und eine Steuerung mit ARDUINO Uno R4 mit Wifi



## ⚡ Hardware-Komponenten

### Arduino Uno + Stepper-Treiber System
- **Arduino Uno** (Mikrocontroller)
- **DS3231 RTC** (Realtime Clock für präzise Zeitsteuerung)
- **NEMA Stepper Motor** mit Treiber (für Förderschnecke)
- **Relais-Modul** (Stromversorgung Motor ein/aus)
- **Aktiver Buzzer** (Akustische Warnsignale)
- **Manuelle Trigger-Pins** (Pin 10+11 Kurzschluss)

### 🔌 Verkabelung
```
DS3231 RTC:
├── VCC → Arduino 3.3V (oder 5V)
├── GND → Arduino GND
├── SDA → Arduino A4 (SDA)
└── SCL → Arduino A5 (SCL)

Stepper-Treiber:
├── PUL+ → Arduino Pin 2 (Schrittimpulse)
├── DIR+ → Arduino Pin 3 (Drehrichtung)
├── ENA+ → Arduino Pin 5 (Motor aktivieren)
└── Relais IN → Arduino Pin 8 (Stromversorgung)

Buzzer:
├── VCC → Arduino 5V
├── GND → Arduino GND
└── SIG → Arduino Pin 6

Manueller Trigger:
└── Pin 10 ↔ Pin 11 (Kurzschluss)
```

## 🚀 Software-Features

### ⏰ Zeitgesteuerte Fütterung
- **Zwei täglich programmierbare Fütterungszeiten**
- **Automatische RTC-Synchronisation** bei jedem Upload
- **Tages-Reset** um Mitternacht für Fütterungsflags

### 🔧 Nicht-blockierende State-Machine
- **Asynchrone Steuerung** ohne delay()-Blockierung
- **Robuste Zustandsübergänge**:
  ```
  BEREIT → BUZZER_WARNUNG → WARTE_NACH_BUZZER → 
  RELAIS_AKTIVIERT → MOTOR_LAEUFT → WARTE_NACH_MOTOR → BEREIT
  ```
- **Präzise Timing-Kontrolle** mit millis()

### 🛡️ Sicherheits- und Robustheitsfunktionen
- **Watchdog Timer** (konfigurierbar: 1s, 2s, 4s, 8s)
- **RTC-Fehlerbehandlung** mit Retry-Mechanismus
- **NOTFALL-MODUS** bei RTC-Ausfall (nur manuelle Fütterung)
- **Stepper-Reset** nach jeder Bewegung

### 📢 Debug und Monitoring
- **Minimale RAM-optimierte Serial-Ausgaben**
- **Debug-Modi**: DEBUG_ZEIT, DEBUG_MOTOR, DEBUG_BUZZER
- **System-Status-Anzeige** beim Start
- **Optionale Temperaturanzeige** (RTC-Sensor)

### 📱 Manuelle Bedienung
- **Hardware-Trigger** durch Kurzschluss Pin 10+11 // Da keine Hardware verfügbar war, musste eine temporäre Lösung her. Ein Pushbutton wird die Behelfslösung später ersetzen.
- **Sofortige Fütterung** unabhängig von Zeitplänen

## 🔧 Installation & Setup

### Bibliotheks-Abhängigkeiten
```bash
# Arduino IDE Library Manager:
- RTClib (Adafruit)
- AccelStepper
- Adafruit BusIO (für RTClib)
```

### ⚙️ Konfiguration
Wichtige Einstellungen in `KORN-Motorsteuerung_simpel.ino`:

```cpp
// Fütterungszeiten
#define FUETTERUNG_STUNDE_1    8    // Erste Fütterung: 08:00
#define FUETTERUNG_MINUTE_1    0
#define FUETTERUNG_STUNDE_2    16   // Zweite Fütterung: 16:00
#define FUETTERUNG_MINUTE_2    0

// Motor-Parameter
#define MOTOR_SCHRITTE         800  // Anzahl Schritte pro Fütterung
#define MOTOR_RPM              60   // Geschwindigkeit (U/min)
#define MOTOR_RECHTS           true // Drehrichtung

// Buzzer-Warnung
#define BUZZER_VORWARNUNG      5    // Wartezeit nach Buzzer (Sekunden)
#define BUZZER_ANZAHL_TOENE    3    // Anzahl Warntöne

// Debug-Modi
#define DEBUG_ZEIT             false
#define DEBUG_MOTOR            false
#define DEBUG_BUZZER           false
#define ZEIGE_TEMPERATUR       false

// Sicherheit
#define WATCHDOG_AKTIV         true
#define WATCHDOG_TIMEOUT       WDTO_8S
```

## 📋 Neueste Verbesserungen (2025)

### ✅ RAM-Optimierung
- **F() Makro** für alle String-Literale (Flash- statt RAM-Speicher)
- **Drastisch verkürzte Serial-Ausgaben** (stichpunktartig)
- **Beseitigung redundanter Debug-Meldungen**
- **Speicher-effiziente Debug-Helper-Funktionen**

### ✅ Code-Robustheit
- **Verbesserte RTC-Initialisierung** mit Retry-Logik
- **Konsistente Variablennamen** und Funktionsaufrufe
- **Optimierte State-Machine** für Fütterungsvorgang
- **Watchdog-Integration** für Systemstabilität

### ✅ Benutzerfreundlichkeit
- **Kompakte System-Status-Ausgabe**
- **Einfache Hardware-Trigger-Funktion**
- **Klare Pin-Dokumentation**
- **Konfigurierbare Debug-Level**

## 💡 Typische Serial-Ausgaben

```
K.O.R.N. INIT
RTC...
RTC OK
10.7.2025 0:19:31
Time sync OK
10.7.2025 0:19:31
Manual: Pin 10+11
Feed: 8:0/16:0
WDT:8s
READY

# Bei Fütterung:
FEED 1
WARN
WARN END
WAIT 5s
REL ON
MOT ON
RUN 800 steps
MOT OFF
REL OFF
DONE
```

## 🏗️ Hardware-Design

Die Bauteile der Förderschnecke wurden mittels **FreeCAD** (LGPL2+, CC-BY-3.0) entworfen. Die Förderschnecke wurde **steckbar** entworfen, somit ist es möglich, die Schnecke auch auf kleineren 3D-Druckern zu drucken.

**Konstruktionsprinzipien:**
- **Robuste KG-/HT-Rohre** als Gehäuse
- **Mäuse- und insektensicher**
- **Wasserdicht und witterungsbeständig**
- **Einfach zu reinigende Komponenten**
- **Standardisierte, verfügbare Bauteile**

## 🔧 Wartung & Troubleshooting

### Häufige Probleme
1. **RTC FAIL!** → DS3231 Verkabelung prüfen, Batterie wechseln
2. **Motor läuft nicht** → Relais/Treiber-Verkabelung prüfen
3. **Keine Fütterung zur Zeit** → RTC-Zeit und Fütterungszeiten prüfen
4. **RAM-Überlauf** → Debug-Modi deaktivieren, Serial-Ausgaben reduzieren

### Manuelle Fütterung
Pin 10 und Pin 11 kurz verbinden (z.B. mit Drahtbrücke) → Sofortige Fütterung wird ausgelöst.

## 🤖 Entwicklung

Der Code wurde mit Hilfe von **künstlicher Intelligenz** entworfen, optimiert und systematisch verbessert. Das Projekt folgt **Open-Source-Prinzipien** und ist vollständig dokumentiert.

### Projektstruktur
```
K.O.R.N./
├── main/
│   └── KORN-Motorsteuerung_simpel/
│       └── KORN-Motorsteuerung_simpel.ino
├── hardware/
│   └── [3D-Modelle, Schaltpläne]
└── README.md
```

---

**Status:** ✅ Produktionsreif | **Version:** 2025.1 | **Lizenz:** Open-Source Hardware/Software
