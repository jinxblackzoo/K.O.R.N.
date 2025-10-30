# K.O.R.N.
**Katastrophal Organisierter Runder Nahrungsmittelspender**

K.O.R.N. ist ein robuster, Open-Source/Hardware, wasserdichter, mit einfachen Mitteln konstruierter und mäusesicherer Fütterungsautomat für Geflügel oder alle andere Art von Hausgetier 😉

## 🎯 Projektübersicht

Aufgrund der enttäuschenden Erfahrung mit gekauften Fütterungsautomaten welche trotz der teils hohen Preise entweder nach drei Wochen defekt waren, oder ganze Mäusefamilien durchfütterten, musste eine Eigenkonstruktion her. Die Entscheidungsgrundlage für das gewählte System mit einer Förderschnecke in einem Rohr, basiert auf einer Recherche im Dubbel Ausgabe von 2001.

Es handelt sich um einen **Stetigförderer (Schnecke)**, der Schüttgut (Futter) aus einem Silo (KG-Rohr) in einen Auswurfschacht befördert. Das Gehäuse besteht aus überall erhältlichen, robusten und günstigen HT-, bzw KG-Rohren.

Revision 3 wurde entwickelt als autarke Off-Grid Funktion. Ein WLAN mit Internetverbindung und somit die Option von überall auf der Welt mittels VPN zu füttern ist hier nicht vorgesehen. Stattdessen kann KORN hier mitten im Feld ohne eigene Internetverbindung installiert werden. Der Arduino UNO R4 spannt dann einen eigenen WLAN Accesspoint auf. Mit diesem kann man sich dann mittels Smartphone verbinden. 
Tipp: Alte Smartphones ohne Internet/Benutzerdaten einfach auf Werkseinstellungen zurücksetzen. Dann mit dem Wlan KORN verbinden und im Browser 192.168.4.1 öffnen. Voila, fertig ist das eigene CCCC (ChickenCoopControlCenter) 🥳 😉

## ⚡ Hardware-Komponenten

### Arduino Uno + Stepper-Treiber System
- **Arduino Uno R4 Wifi** (Mikrocontroller)
- **DS1302 RTC** (Realtime Clock für präzise Zeitsteuerung)
- **NEMA Stepper Motor** Antrieb
- **DM320T Stepper-Motor Treiber** Motortreiber
- **Relais-ModulJQC3F oder ähnliches  NO/COM/NC** (Stromversorgung Motor ein/aus)
- **Aktiver Buzzer** (Akustische Warnsignale)
- **Pushbutton momentarily** (Pin 10+11 Kurzschluss)

### 🔌 Verkabelung
```
DS1302RTC:
├── VCC     → Arduino 5V ⚠️ WICHTIG: NICHT 3,3V!
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
* **DS1302**: 
  - ⚠️ **KRITISCH**: VCC MUSS an 5V! Mit 3,3V funktioniert die RTC nicht und verursacht Bootloops!
  - CR2032-Stützbatterie einsetzen (~5 Jahre Lebensdauer)
  - Gemeinsame Masse (GND) mit Arduino und Peripherie sicherstellen
* **DM320T** (Common-Cathode): 
  - PUL−, DIR−, ENA− an Arduino GND
  - OPTO nicht am Arduino verwenden -> Muss an +5V Versorgung
* **Relais**: 
  - HW-482: Typischerweise ACTIVE-HIGH (HIGH=EIN, LOW=AUS)
  - Relais trennt nur die Plusleitung: 12V+ → COM → NO → +Vdc des DM320T
* **Manueller Trigger**: 
  - Pin 11 als permanentes LOW (OUTPUT)
  - Pin 10 als INPUT_PULLUP
  - Kurzschluss 10 ↔ 11 löst sofort aus (mit 150ms Entprellung)
* **ENA-Logik**: ENA=HIGH aktiviert den Treiber; ENA=LOW deaktiviert

## 🚀 Software-Features

### ⏰ Intelligente Fütterungslogik
* **Automatischer Zeitplan**: Bis zu 2 Fütterungszeiten täglich
* **Sicherheitsabstand**: Mindestens 2 Minuten zwischen Fütterungen
* **Verzögerungsanzeige**: Bei zu häufigen Fütterungsversuchen wird die exakte Wartezeit angezeigt
* **Erweiterte Laufzeit**: 1–600 Sekunden (10 Minuten) für unterschiedliche Futtermengen
* **Live-Updates**: Webseite zeigt aktuelle Uhrzeit und letzte Fütterung in Echtzeit

### 💾 Konfigurationsspeicherung & Datensicherheit
* **Doppelte Datensicherung**: Konfiguration wird parallel in RTC-RAM UND EEPROM gespeichert
* **Automatisches Backup**: Bei jedem Speichervorgang wird EEPROM aktualisiert
* **Batterie-Ausfallschutz**: Bei CR2032-Ausfall wird Config aus EEPROM wiederhergestellt
* **Status-Anzeige**: 
  - **CFG:RAM** = Aus RTC-RAM geladen (normal)
  - **CFG:EEPROM** = Aus EEPROM wiederhergestellt (nach Batterieausfall)
  - **CFG:DEF** = Standard-Werte (erster Start)
* **EEPROM-Lebensdauer**: ~100.000 Schreibzyklen (bei normaler Nutzung jahrelang haltbar)

### 📱 Benutzerfreundliche Bedienung

* Eigenes WLAN (Access Point): SSID "KORN", Passwort "Chaosfeeder"
  Eigenes PW kann später im Script gesetzt werden!!
* Einfache Handy-Webseite (mobilfreundlich):
  - Zwei Fütterungszeiten einstellen
  - Zweite Fütterungszeit deaktivieren
  - Motor-Laufzeit in Sekunden festlegen (1–600 Sekunden = bis zu 10 Minuten)
  - Countdown bis zur nächsten Fütterung mit Live-Uhr
  - Button "Jetzt füttern" mit 2-Minuten-Mindestabstand
  - Intelligente Verzögerungsanzeige: Zeigt exakte Uhrzeit bei Mindestabstand-Verzögerung
  - Letzte Fütterung wird live aktualisiert (Zeit + Quelle: Manuell/Web/Timer)
  - Warn-Button „Blockade lösen (Rechtslauf)" mit Laufzeit-Eingabe (Standard 2 s; Begrenzung 1–60 s). Nur kurzfristig verwenden!
  - Button „Interne Uhr aktualisieren" (setzt die interne RTC auf die Gerätezeit des Browsers/Clients)
  - Robuste HTTP-Header: No-Cache, Connection: close, Sicherheits-Header (X-Content-Type-Options, X-Frame-Options, Referrer-Policy)
  - 303 Redirect nach Formularaktionen (verhindert doppeltes Absenden bei Reload)
  - Footer mit GitHub-Link und Build-Datum (vom letzten Sketch-Build über __DATE__/__TIME__)
  - Batteriestatus (CR2032) als Anzeige-Button: Grün=OK, Rot=Bitte CR2032 tauschen (Heuristik: RTC-Zeit + RTC-RAM)
* ✅ Manuelle Bedienung:
  - ✅ Kurzschluss Pin 10 ↔ 11 als Taster
  - ✅ Sofortige Fütterung unabhängig vom Zeitplan
* ✅ Uhrzeit-/Zeitzonen-Handling ohne Internet: Beim Upload wird die RTC auf die lokale PC-Zeit gesetzt (keine automatische Sommer-/Winterzeit-Umstellung)
* ✅ Tages-Reset um Mitternacht
* ✅ Stepper-Reset nach jeder Bewegung

> Hinweis: AP-Insellösung (Off-Grid)
> - KORN stellt ein eigenes WLAN bereit und nutzt im AP-Modus typischerweise 192.168.4.1/24; die tatsächliche IP wird beim Start seriell ausgegeben.
> - Für die Nutzung einfach mit dem WLAN "KORN" verbinden und im Browser `http://192.168.4.1` öffnen.
> - Währenddessen besteht in der Regel keine Internetverbindung; das vermeidet Konflikte mit Heimnetzwerken.
> - Auf Smartphones: __Mobile Daten deaktivieren__ (sonst bevorzugen viele Geräte LTE/5G und die Seite lädt nicht).
> - Eventuell müssen VPN Verbindungen deaktiviert werden.
> - Während der Motor läuft, wird die Seite nicht neu geladen, sondern bleibt quasi im Lademodus "stecken". Dies ist keine Fehlfunktion, sondern eine bewußte Designentscheidung um den Code simpel zu halten. Das Arduino arbeitet seine Aufgaben sequenziell (Eine nach der anderen) ab. Sobald der Motor stoppt, lädt die Seite wieder. 



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
* Standard-Laufzeit: 5 s (erweitert auf 1–600 s = bis zu 10 Minuten für große Hühnerscharen)
* Blockadelöser (UI-Default): 2 s (nicht persistent, nur Eingabewert in der Seite)
* Mindestabstand zwischen Fütterungen: 2 Minuten (Sicherheitsfeature)

## 💾 Persistenz & Batterie

* Konfiguration wird im batteriegepufferten RAM der **DS1302** gespeichert und übersteht Stromausfälle bei intakter **CR2032**.
* Ist die Batterie leer/fehlend oder werden Daten korrupt, lädt das Gerät **Werkseinstellungen** (Defaults).
* Heuristischer Batterietest: Batterie gilt als „OK“, wenn die **RTC-Zeit gültig** ist und die **Konfiguration aus RTC‑RAM** geladen wurde. Die Web‑UI zeigt einen Anzeige‑Button (Grün/Rot).
* Indikatoren in der seriellen Ausgabe: `RTC: OK/--`, `CFG: RAM/DEF`.

---

### 🖥️ Serieller Monitor (für Experten)
- Baudrate: __115200 Baud__
- Datenbits/Parität/Stoppbits: __8-N-1__ (Standard)
- Zeilenende: __No line ending__ (keine Eingaben erforderlich)
- Port: das ACM-Gerät des Boards (z. B. `/dev/ttyACM0`)

## 💡 Was zeigt der serielle Monitor?

Der serielle Monitor ist hauptsächlich für Entwickler und Fehlersuche gedacht. Er zeigt:

• **WLAN-Zugangsdaten**: SSID "KORN" und Passwort zum Verbinden
• **Aktuelle Uhrzeit**: Datum und Uhrzeit der internen Uhr
• **Letzte Fütterung**: Wann zuletzt gefüttert wurde und wie lange der Motor lief
• **Nächste Fütterung**: Countdown bis zur nächsten automatischen Fütterung
• **Batteriestatus**: Ob die CR2032-Batterie der Uhr noch funktioniert
• **Betriebszeit**: Wie lange das Gerät bereits läuft

**Beispiel einer Statuszeile:**
```
AP:KORN Pw:Chaosfeeder IP:192.168.4.1 | 2025-08-17 18:48 | Last:17:10 | Next1:19:00(00:12) | RTC:OK | Up:00:32
```

**Bedeutung:**
- Mit WLAN "KORN" verbinden, dann Browser auf 192.168.4.1 öffnen
- Aktuelle Zeit: 18:48 Uhr am 17. August 2025
- Letzte Fütterung: um 17:10 Uhr
- Nächste Fütterung: um 19:00 Uhr (in 12 Minuten)
- Batterie der Uhr: OK
- Gerät läuft seit 32 Minuten

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

## Wartung & Troubleshooting

### Häufige Probleme und Lösungen

#### Webseite lädt nicht
* **"Kein Internet" Meldung**: Normal bei Off-Grid-Betrieb - einfach ignorieren
* **Seite lädt nicht**: 
  - Mobile Daten am Handy ausschalten
  - Sicherstellen, dass mit WLAN "KORN" verbunden (nicht Heim-WLAN)
  - Browser auf `http://192.168.4.1` öffnen

#### WLAN-Probleme  
* **WLAN "KORN" wird nicht angezeigt**: 
  - Näher an das Gerät herangehen
  - Gerät neu starten (Strom aus/ein)
  - Andere WLAN-Geräte kurz ausschalten

#### Fütterung funktioniert nicht
* **"Jetzt füttern" reagiert nicht**: 
  - 2 Minuten seit letzter Fütterung warten
  - Webseite zeigt dann exakte Wartezeit an
* **Motor dreht nicht**: 
  - 12V-Netzteil angeschlossen und eingeschaltet?
  - Alle Kabel fest verbunden?
  - Grünes Lämpchen am Arduino leuchtet?

#### Batterie-Probleme
* **Rote Batterie-Anzeige**: CR2032 in der Uhr tauschen
* **Einstellungen gehen verloren**: Neue CR2032 einsetzen, dann neu konfigurieren

### Notfall-Fütterung (Hardware-Taster)
Falls die Webseite nicht funktioniert: Pin 10 und Pin 11 am Arduino kurz mit einem Draht verbinden → Sofortige Fütterung wird ausgelöst (umgeht 2-Minuten-Regel).

---

## ⚡ Stromverbrauch & Solarbetrieb

Für den autarken Off-Grid-Betrieb mit Solarstrom sind folgende Verbrauchswerte relevant:

### Gemessene Stromaufnahme bei 12V
- **Ruhemodus** (Motor aus, Relais aus, Arduino an): **0,104 A** (1,25 W)
- **Aktiver Betrieb** (Motor läuft, DM320T aktiv): **0,7 A** (8,4 W)

### DM320T Konfiguration (aktuell)
Die Messwerte basieren auf folgender DIP-Schalter-Einstellung der Variante A:
- SW1=ON, SW2=ON, SW3=OFF, SW4=ON, SW5=ON, SW6=ON
- Entspricht: 1,3A Peak (0,92A RMS), Microstep 2

### Dimensionierung für Solarbetrieb
**Täglicher Energiebedarf (Beispielrechnung):**
- Ruhemodus: 23,5h × 1,25W = 29,4 Wh
- Fütterungen: 2× 10s × 8,4W = 0,05 Wh
- **Gesamt: ~30 Wh/Tag**

**Empfohlene Solaranlage:**
- Solarpanel: 20-30W (je nach Standort/Jahreszeit)
- Akku: 12V/7-12Ah (84-144 Wh Kapazität)
- Laderegler: 12V PWM/MPPT für entsprechende Panel-Leistung

**Beispiel-Konfiguration (getestet):**
- Solarpanel: 130W (deutlich überdimensioniert → sehr zuverlässig)
- Akku: 12V/12Ah Sealed Lead Acid (144 Wh nominal, ~72 Wh nutzbar = 2-3 Tage Autonomie)
- Laderegler: entsprechend 130W Panel dimensioniert

**Hinweise:**
- Bei längeren Fütterungszeiten (>30s) entsprechend höher dimensionieren
- Wintermonate: Größeres Panel oder zusätzliche Akkukapazität einplanen
- Standby-Verbrauch dominiert den Energiebedarf deutlich

---

## Entwicklung

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
  - **Gemessener Verbrauch bei 12V:** 0,7A aktiv, 0,104A Standby
  - **Ideal für Solarbetrieb:** Minimaler Stromverbrauch, ~30 Wh/Tag

- __Variante B (konservativ, guter TQ)__
  - Dynamischer Strom: 1.6A Peak (1.13A RMS)
    - SW1=OFF, SW2=ON, SW3=OFF
  - Microstep 2 (400 Steps/Rev):
    - SW4=ON, SW5=ON, SW6=ON
  - **Gemessener Verbrauch bei 12V:** 0,920A aktiv, 0,105A Standby
  - **Solarbetrieb:** Höherer Verbrauch, ~37 Wh/Tag (23% mehr als Variante A)

- __Variante C (mehr TQ, prüfe Netzteil/Temperatur)__
  - Dynamischer Strom: 1.9A Peak (1.34A RMS)
    - SW1=ON, SW2=OFF, SW3=OFF
  - Microstep 2 (400 Steps/Rev):
    - SW4=ON, SW5=ON, SW6=ON
  - **Gemessener Verbrauch bei 12V:** 1,125A aktiv, 0,105A Standby
  - **Solarbetrieb:** Höchster Verbrauch, ~43 Wh/Tag (43% mehr als Variante A)

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
