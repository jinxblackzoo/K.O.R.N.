# K.O.R.N.
**Katastrophal Organisierter Runder Nahrungsmittelspender**

K.O.R.N. ist ein robuster, Open-Source/Hardware, wasserdichter, mit einfachen Mitteln konstruierter und mäusesicherer Fütterungsautomat für Geflügel.

## 🎯 Projektübersicht

Aufgrund der enttäuschenden Erfahrung mit gekauften Fütterungsautomaten welche trotz der teils hohen Preise entweder nach drei Wochen defekt waren, oder ganze Mäusefamilien durchfütterten, musste eine Eigenkonstruktion her. Die Entscheidungsgrundlage für das gewählte System mit einer Förderschnecke in einem Rohr, basiert auf einer Recherche im Dubbel Ausgabe von 2001.

Es handelt sich um einen **Stetigförderer (Schnecke)**, der Schüttgut (Futter) aus einem Silo (KG-Rohr) in einen Auswurfschacht befördert. Das Gehäuse besteht aus überall erhältlichen, robusten und günstigen HT-, bzw KG-Rohren.

## ⚡ Hardware-Komponenten

### Arduino Uno + Stepper-Treiber System
- **Arduino Uno R4 Wifi** (Mikrocontroller)
- **DS1302 RTC** (Realtime Clock für präzise Zeitsteuerung)
- **NEMA Stepper Motor** mit Treiber (für Förderschnecke)
- **Relais-ModulJQC3F oder ähnliches  NO/COM/NC** (Stromversorgung Motor ein/aus)
- **Aktiver Buzzer** (Akustische Warnsignale)
- **Pushbutton momentarily** (Pin 10+11 Kurzschluss)

### 🔌 Verkabelung
```
DS1302RTC:
├── VCC     → Arduino 3.3V (oder 5V)
├── GND     → Arduino GND
├── CLK     → Arduino Pin 7
└── DAT     → Arduino Pin 9
└── RST     → Arduino Pin 12

Stepper-Treiber DM320T:
├── PUL+    → Arduino Pin 2 (Schrittimpulse)
├── DIR+    → Arduino Pin 3 (Drehrichtung)
├── ENA+    → Arduino Pin 5 (Motor aktivieren)
└── OPTO    → Not used

└── GND   → 12V Jack -
└── +Vdc  → Relay NO
└── A+    → NEMA17  
└── A-    → NEMA17
└── B+    → NEMA17
└── B-    → NEMA17

Buzzer:
├── VCC   → Arduino 5V
├── GND   → Arduino GND
└── SIG   → Arduino Pin 6

Pushbutton:
└── Pin 10 ↔ Pin 11 (Kurzschluss)

Relay: 
└── S     → Arduino Pin 8 
└── +     → Arduino 5V
└── -     → Arduino GND

└── NO    → DM320T +Vdc
└── COM   → 12V Jack +  (Mittlerer Pin auf Leiterbahn)
└── NC    → Not used

```

#### Hinweise zur Verdrahtung
* DS1302: CR2032-Stützbatterie einsetzen; gemeinsame Masse (GND) mit Arduino und Peripherie sicherstellen.
* DM320T (Common-Cathode): PUL−, DIR−, ENA− an Arduino GND; OPTO ungenutzt wie gezeigt.
* Manueller Trigger: Pin 11 als permanentes LOW (OUTPUT), Pin 10 als INPUT_PULLUP; Kurzschluss 10 ↔ 11 löst sofort aus.
* Relais trennt nur die Plusleitung: 12V+ → COM → NO → +Vdc des DM320T.
* ENA-Logik: ENA=HIGH aktiviert den Treiber; ENA=LOW deaktiviert.

## 🚀 Software-Features

* ✅ Eigenes WLAN (Access Point): SSID "KORN", Passwort "Chaosfeeder"
  Eigenes PW kann später im Script gesetzt werden!!
* ✅ Einfache Handy-Webseite (mobilfreundlich):
  - ✅ Zwei Fütterungszeiten einstellen
  - ✅ Zweite Fütterungszeit deaktivieren
  - ✅ MOTOR_SCHRITTE festlegen
  - ✅ Countdown bis zur nächsten Fütterung
  - ✅ Button "Jetzt füttern"
* ✅ Manuelle Bedienung:
  - ✅ Kurzschluss Pin 10 ↔ 11 als Taster
  - ✅ Sofortige Fütterung unabhängig vom Zeitplan
* ✅ Uhrzeit-/Zeitzonen-Handling ohne Internet: Beim Upload wird die RTC auf die lokale PC-Zeit gesetzt (keine automatische Sommer-/Winterzeit-Umstellung)
* ✅ Tages-Reset um Mitternacht
* ✅ Stepper-Reset nach jeder Bewegung

> Hinweis: AP-Insellösung (Off-Grid)
> - KORN stellt ein eigenes WLAN bereit und nutzt im AP-Modus fest die IP 192.168.4.1/24.
> - Für die Nutzung einfach mit dem WLAN "KORN" verbinden und im Browser `http://192.168.4.1` öffnen.
> - Währenddessen besteht in der Regel keine Internetverbindung; das vermeidet Konflikte mit Heimnetzwerken.
> - Auf Smartphones: __Mobile Daten deaktivieren__ (sonst bevorzugen viele Geräte LTE/5G und die Seite lädt nicht).

#### 🔐 Zugangsdaten (SSID/Passwort)
- Standard-SSID: `KORN`
- Standard-Passwort: `Chaosfeeder`

Wo ändern?
- Datei: `KORN_Rev3/Script_KORN_REV3/main/main.ino`
- In `setup()` wird der Access Point gestartet:

```cpp
// Stelle hier SSID/Passwort ein
apInit("KORN", "Chaosfeeder");
```

Hinweise:
- Nach Änderung neu kompilieren/flashen.
- SSID/PW auch in der seriellen Startmeldung ausgegeben (zum schnellen Verbinden).
- Sicherheit: Für den Einsatz in fremden Umgebungen eigenes Passwort wählen.
- WLAN-Sicherheit: WPA2-PSK. __Kein zusätzlicher Web-Login__ auf der Seite nötig; die Zugangsdaten gelten nur für das WLAN.

### 🧰 Standardwerte
* Fütterungszeit 1: 07:01
* Fütterungszeit 2: 16:01 (aktiv)
* MOTOR_SCHRITTE: 2000

---

### 🖥️ Serieller Monitor (Einstellungen)
- Baudrate: __115200 Baud__
- Datenbits/Parität/Stoppbits: __8-N-1__ (Standard)
- Zeilenende: __No line ending__ (keine Eingaben erforderlich)
- Port: das ACM-Gerät des Boards (z. B. `/dev/ttyACM0`)


## 💡 Typische Serial-Ausgaben

• SSID/PW und AP-IP zum schnellen Verbinden
• Datum/Uhrzeit (RTC)
• Letzte Fütterung (Zeit, Schritte)
• Nächste Fütterung(en) inkl. Countdown
• Plan-Status (zweite Zeit aktiv/deaktiv)
• MOTOR_SCHRITTE (aktuelle Einstellung)
• RTC/Config-Status (RTC OK/FAIL, Config aus RAM/Defaults)
• Uptime

Ausgabe-Rhythmus:
- Beim Start: eine vollständige Statuszeile
- Danach: alle 60 Sekunden eine kompakte Statuszeile

Beispiel (eine Zeile):

```
AP:KORN Pw:Chaosfeeder IP:192.168.4.1 | 2025-08-17 18:48 | Last:17:10(800) | Next1:19:00(00:12) Next2:- [off] | Steps:800 | RTC:OK CFG:RAM | Up:00:32
```

Legende:
- AP: SSID, Pw: Passwort, IP: AP-IP des Geräts
- Last: letzte Fütterung HH:MM (Schritte)
- Next1/Next2: nächste Fütterungszeit (Countdown MM:SS); „[off]“ = zweite Zeit deaktiviert
- Steps: `MOTOR_SCHRITTE`
- RTC: OK/FAIL; CFG: RAM = aus DS1302-RAM geladen, DEFAULT = Fallback-Werte
- Up: Betriebszeit (hh:mm)

---

## 🏗️ Hardware-Design

Die Bauteile der Förderschnecke wurden mittels FreeCAD (LGPL2+, CC-BY-3.0) entworfen. Die Förderschnecke wurde steckbar entworfen, somit ist es möglich, die Schnecke auch auf kleineren 3D-Druckern zu drucken.

**Konstruktionsprinzipien:**
- **Robuste KG-/HT-Rohre** als Gehäuse
- **Mäuse- und insektensicher**
- **Wasserdicht und witterungsbeständig**
- **Einfach zu reinigende Komponenten**
- **Standardisierte, verfügbare Bauteile**

---

## 🔧 Wartung & Troubleshooting

### Häufige Probleme
* **AP ohne Internet**: Im Insellösungs-Modus normal. Browser-Hinweis „kein Internet“ ignorieren oder mobile Daten kurz deaktivieren.
* **Kein Zugriff auf 192.168.4.1**:
  - Sicherstellen, dass das Gerät mit „KORN“ verbunden ist (nicht im Heim-WLAN).
  - Mobile Daten aus, Seite neu laden.
  - SSID/Passwort in der seriellen Ausgabe prüfen.
* **IP-Konflikt 192.168.4.0/24**:
  - Keine parallele Heimnetz-Verbindung im gleichen Subnetz verwenden.
  - Entweder bewusst nur mit „KORN“ verbinden (Insellösung) oder Heimnetz auf anderes Subnetz umstellen.
* **AP wird nicht angezeigt**: Gerät neu starten, SSID „KORN“ prüfen, näher an das Gerät herangehen.
* **Manuelle Fütterung reagiert nicht**: Kurzschluss Pin 10 ↔ 11 sicher herstellen; Pin 10 ist INPUT_PULLUP und muss auf GND gezogen werden.
* **Motor läuft nicht**: Relais (D8) schaltet 12V? Gemeinsame Masse vorhanden? ENA (D5) aktiviert? DM320T-Versorgung 10–30V geprüft?

### 🖲️ Manuelle Fütterung
Pin 10 und Pin 11 kurz verbinden (z.B. mit Drahtbrücke) → Sofortige Fütterung wird ausgelöst.

---

## 🤖 Entwicklung

Der Code wurde mit Hilfe von **künstlicher Intelligenz** entworfen, optimiert und systematisch verbessert. Das Projekt folgt **Open-Source-Prinzipien** und ist vollständig dokumentiert.

### 🗂️ Projektstruktur
```
KORN_Rev3/
├── Script_KORN_REV3/
│   └── main/
│       ├── main.ino
│       ├── homepage.ino
│       ├── ap.ino
│       └── motor.ino
├── pics_video_additional-info/
│   └── DM320T_user_manual.txt
└── README.md
```

### ⚙️ DM320T DIP‑Schalter (Strom/Microstep)

Siehe `pics_video_additional-info/DM320T_user_manual.txt` (Abschnitt 7.1/7.2). Standstill-Current wird nach ~0,4 s automatisch auf 50% reduziert.

Varianten (konservativ → kräftiger), jeweils mit Microstep 2 für robustes Drehmoment bei Anlauf:

- __Variante A (sehr netzteilschonend, verbreitet)__
  - Dynamischer Strom: 1.3A Peak (0.92A RMS)
    - SW1=ON, SW2=ON, SW3=OFF
  - Microstep 2 (400 Steps/Rev):
    - SW4=ON, SW5=ON, SW6=ON

- __Variante B (konservativ, guter TQ)__
  - Dynamischer Strom: 1.6A Peak (1.13A RMS)
    - SW1=OFF, SW2=ON, SW3=OFF
  - Microstep 2 (400 Steps/Rev):
    - SW4=ON, SW5=ON, SW6=ON

- __Variante C (mehr TQ, prüfe Netzteil/Temperatur)__
  - Dynamischer Strom: 1.9A Peak (1.34A RMS)
    - SW1=ON, SW2=OFF, SW3=OFF
  - Microstep 2 (400 Steps/Rev):
    - SW4=ON, SW5=ON, SW6=ON

Hinweise:
- DM320T max. 2.2A Peak – nicht empfohlen bei 2A-Netzteilgrenze.
- Microstepping glättet Lauf, „per-Step“-Moment sinkt. Für hohe Durchsetzung unter Last sind 2 oder 4 Microsteps sinnvoll.
- Falls Motor nur „hält“: In `motor.ino` ggf. `ENABLE_ACTIVE_HIGH=false` und/oder `INVERT_STEP=true` setzen; gemeinsame Masse prüfen.

__Wann welche Variante?__
- **Variante A (1.3A Peak)**: Kleines/empfindliches Netzteil (≤2A), Dauerbetrieb/hohe Einschaltdauer, geringe bis mittlere Last. Fokus auf kühlen, zuverlässigen Betrieb.
- **Variante B (1.6A Peak)**: Standardempfehlung für K.O.R.N. mit 2A-Netzteil. Mittlere Last, kurze Laufzeiten (Fütterungszyklen), gutes Drehmoment bei moderater Erwärmung.
- **Variante C (1.9A Peak)**: Höhere Last/Anlaufmomente (z. B. gegen Futterbrücken) bei kurzen Einsätzen, gute Belüftung. Netzteil muss stabil ≥2A liefern. Nicht für langen Dauerbetrieb.

Tipps:
- Steigen Schritte aus → eine Stromstufe höher. Wird Treiber/Motor zu warm → eine Stufe runter.
- Bei rauem Lauf/Resonanzen Microstep auf 4 (SW4=OFF, SW5=ON, SW6=ON) testen.

### 🔌 DM320T Verkabelung (OPTO/PUL/DIR/ENA)

Der DM320T hat einzelne Logikeingänge `PUL`, `DIR`, `ENA` und eine gemeinsame Versorgungs‑Klemme `OPTO` (+5V für die Optokoppler). Empfehlung: „Common‑Anode“.

Verdrahtung (entspricht den Pins im Sketch `motor.ino`):

```
DM320T OPTO  → Arduino 5V  (aufteilen per Y‑Kabel/Breadboard; alternativ 5V‑Buck vom 12V‑Netzteil)
DM320T PUL   → Arduino D2
DM320T DIR   → Arduino D3
DM320T ENA   → Arduino D5   (zum Testen kann ENA unbeschaltet bleiben; Treiber ist i.d.R. enabled)

DM320T +Vdc  → Motornetzteil + (z. B. 12–24V)
DM320T GND   → Motornetzteil −
Arduino GND  ↔ DM320T GND (sternförmig verbinden; gemeinsame Masse als Referenz)

DM320T A+, A−, B+, B− → Motorphasen gemäß Motordatenblatt
```

Hinweise:
- `OPTO` ist der gemeinsame Plus für die Optokoppler‑LEDs. Die Arduino‑Pins „sinken“ Strom über `PUL/DIR/ENA` nach GND.
- VIN am Arduino ist ungeeignet für `OPTO`. Wenn 5V‑Pin belegt ist: 5V aufsplitten oder kleinen 5V‑Step‑Down (Buck) vom 12V‑Zweig nutzen und dessen GND mit Arduino‑GND verbinden.
- Signalführung kurz, sauber, möglichst verdrillt (z. B. PUL↔GND, DIR↔GND, ENA↔GND).

Passende Software‑Einstellungen (bereits im Code vorgesehen):
- In `KORN_Rev3/Script_KORN_REV3/main/motor.ino` bei dieser Verdrahtung typischerweise:
  - `ENABLE_ACTIVE_HIGH = false`
  - `INVERT_STEP = true`
  - `INVERT_DIR` je nach Drehrichtung (falls invertiert → `true`)

### 📦 Arduino IDE: Benötigte Pakete & Bibliotheken

* __Board-Paket (Boards Manager)__
  - Arduino UNO R4 Boards (Renesas RA) – Board: „Arduino UNO R4 WiFi“ auswählen

* __Bibliotheken (Library Manager)__
  - AccelStepper (Autor: Mike McCauley / AirSpayce)
    - Stepper-Ansteuerung für den DM320T (`#include <AccelStepper.h>`) 
  - WiFiS3 (Autor: Arduino)
    - Wird für den Access-Point und den Webserver benötigt (`#include <WiFiS3.h>`)
  - Rtc (Autor: Makuna)
    - Unterstützt DS1302-RTC inkl. RAM-Nutzung (`RtcDS1302`/Makuna-Library)

Hinweise:
- Installation jeweils über „Werkzeuge → Bibliotheken verwalten…“, nach den oben genannten Namen suchen.
- Nach Installation Board „Arduino UNO R4 WiFi“ wählen und den richtigen COM/TTY-Port einstellen.
 - Falls AccelStepper nicht gelistet ist: ZIP installieren über „Sketch → Include Library → Add .ZIP Library…“
   - ZIP: https://github.com/airspayce/AccelStepper/archive/refs/heads/master.zip

---

**Status:** In Entwicklung | **Version:** 2025.8 | **Lizenz:** Open-Source CAD-Hardware/Software
