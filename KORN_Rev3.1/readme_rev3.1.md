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
- **DS3231 RTC** (Realtime Clock für präzise Zeitsteuerung, I2C)
- **NEMA Stepper Motor** Antrieb
- **DM320T Stepper-Motor Treiber** Motortreiber
- **Relais-ModulJQC3F oder ähnliches  NO/COM/NC** (Stromversorgung Motor ein/aus)
- **Aktiver Buzzer** (Akustische Warnsignale)
- **Pushbutton momentarily** (Pin 10+11 Kurzschluss)

### 🔌 Verkabelung
```
DS3231 RTC:
├── VCC     → Arduino 5V ⚠️ WICHTIG: NICHT 3,3V!
├── GND     → Arduino GND
├── SDA     → Arduino Pin A4
└── SCL     → Arduino Pin A5


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
* **DS3231**: 
  - VCC an Arduino 5V (das DS3231-IC selbst arbeitet 2,3–5,5V, viele Module sind aber auf 5V-Pegel ausgelegt)
  - SDA an A4, SCL an A5 (I2C)
  - Gemeinsame Masse (GND) mit Arduino und Peripherie sicherstellen
  - ⚠️ **Backup-Batterie**: Viele DS3231-Module laden die Backup-Zelle. Verwende nur eine **wiederaufladbare LIR2032** (inkl. passender Ladeschaltung) oder bau die Ladeschaltung aus. Eine nicht wiederaufladbare **CR2032** kann bei dauerhaftem Laden auslaufen, heiß werden oder platzen.
  - 📌 **TODO / Ersatzteil:** Das RTC-Modul soll gegen ein **ZS-042 (DS3231)** mit **LIR2032** getauscht werden — die LIR2032 ist wiederaufladbar und wird über die onboard-Ladeschaltung des ZS-042 geladen; der periodische Batteriewechsel entfällt damit. Anschluss identisch (I2C: SDA=A4, SCL=A5, VCC=5V).
* **DM320T** (Common-Cathode, Standard): 
  - `OPTO` → Arduino GND
  - `PUL+` → Arduino D2, `PUL−` → GND
  - `DIR+` → Arduino D3, `DIR−` → GND
  - `ENA+` → Arduino D5, `ENA−` → GND
  - `+Vdc` / `GND` → Motornetzteil 12V
  - `A+`, `A−`, `B+`, `B−` → NEMA17
  - In `motor.ino` bereits eingestellt: `ENABLE_ACTIVE_HIGH = true`, `INVERT_STEP = false`
* **Alternativ: Common-Anode** (nur falls dein Modul/Layout das so erfordert): 
  - `OPTO` → Arduino 5V
  - `PUL−` → Arduino D2, `PUL+` → 5V
  - `DIR−` → Arduino D3, `DIR+` → 5V
  - `ENA−` → Arduino D5, `ENA+` → 5V
  - Dann `ENABLE_ACTIVE_HIGH = false` und `INVERT_STEP = true` setzen
* **Relais**: 
  - HW-482: Kann ACTIVE-HIGH oder ACTIVE-LOW sein (je nach Modul-Variante)
  - **Test:** LED am Modul leuchtet bei HIGH = ACTIVE-HIGH, bei LOW = ACTIVE-LOW
  - Im Code anpassbar: `RELAY_ACTIVE_HIGH` in `motor.ino` (Standard: `true`)
  - Relais trennt nur die Plusleitung: 12V+ → COM → NO → +Vdc des DM320T
* **Manueller Trigger**: 
  - Pin 11 als permanentes LOW (OUTPUT)
  - Pin 10 als INPUT_PULLUP
  - Kurzschluss 10 ↔ 11 löst sofort aus (mit 150ms Entprellung)
* **ENA-Logik**: ENA=HIGH aktiviert den Treiber; ENA=LOW deaktiviert (Common-Cathode, Pin gibt Strom)

## 🚀 Software-Features

### ⏰ Intelligente Fütterungslogik
* **Automatischer Zeitplan**: Bis zu 2 Fütterungszeiten täglich
* **Sicherheitsabstand**: Mindestens 2 Minuten zwischen Fütterungen
* **Verzögerungsanzeige**: Bei zu häufigen Fütterungsversuchen wird die exakte Wartezeit angezeigt
* **Erweiterte Laufzeit**: 1–600 Sekunden (10 Minuten) für unterschiedliche Futtermengen
* **Live-Updates**: Webseite zeigt aktuelle Uhrzeit und letzte Fütterung in Echtzeit
* **Nachhol-Fenster** (aus Rev2.1): War das Gerät zur Zielzeit aus (z. B. Strom-/Solar-Ausfall), wird innerhalb von **60 Minuten** nach der Zielzeit einmalig nachgefüttert. Der Zustand „heute schon gefüttert" liegt persistent im EEPROM und verhindert Doppelfütterung nach Reboot.
* **Tagesmarker sichtbar & rücksetzbar**: Die Startseite zeigt `Heute gefüttert: 1:ja/nein 2:ja/nein`. Ein bereits gesetzter Marker unterdrückt weitere Auslösungen desselben Termins am selben Tag — **auch nach Zeitumstellung** (Marker überleben Reboots und Reflashes). Ein unterdrückter Termin landet als Eintrag im Event-Log („SCHEDx unterdrueckt: heute gefuettert"). Für Tests/Zeitumstellungen gibt es den Button **„Tagesmarker zurücksetzen (Test)"** (`POST /feedreset`) — er entspricht dem EEPROM-Reset-Jumper aus Rev2.1.

### 💾 Konfigurationsspeicherung & Datensicherheit
* **EEPROM-Speicher**: Konfiguration wird im Arduino-EEPROM gespeichert
* **Fütterungszustand persistent**: Letzte Fütterung, „heute-gefüttert"-Marker und Datum liegen zusätzlich als geprüfter Datensatz (Magic + CRC) im EEPROM – nach Stromausfall/Reboot weiss das Gerät, ob bereits gefüttert wurde
* **Batterie-unabhängige Konfiguration**: Konfiguration und Fütterungszustand liegen im EEPROM — eine leere CR2032 betrifft nur die RTC-Zeit, nicht die gespeicherten Einstellungen
* **Status-Anzeige**: 
  - **CFG:EEPROM** = Aus EEPROM geladen (normal)
  - **CFG:DEF** = Standard-Werte (erster Start)
* **EEPROM-Lebensdauer**: ~100.000 Schreibzyklen (bei normaler Nutzung jahrelang haltbar)

### 🛡️ 24/7-Betriebssicherheit
* **Hardware-Watchdog (4 s)**: Nutzt `WDT.h` des UNO R4. Aktiviert erst nach dem Boot (WiFi-Init kann >4 s blockieren), Refresh in `loop()` und während des Motorlaufs. Bei Code-Hänger → automatischer Reboot. Abschaltbar über `#define WATCHDOG_ENABLED`
* **I2C-Timeout**: `Wire.setWireTimeout(3000)` – eine gestörte RTC-Verbindung blockiert den Betrieb max. 3 s statt ewig
* **Erweiterte RTC-Plausibilität**: Alle Datumsfelder (Jahr/Monat/Tag/h/m/s) werden geprüft, nicht nur das Jahr – I2C-Müllwerte werden erkannt
* **Build-Zeit-Sanity**: Liegt die RTC-Zeit deutlich hinter der Kompilierzeit (Zurücksetzen), wird sie auf die Build-Zeit korrigiert
* **Boot ohne USB**: Wartet max. 2 s auf serielle Verbindung – kein Hängen im Feld
* **millis()-Fallback**: Ohne erreichbare RTC läuft die Uhr per `millis()` weiter, RTC-Retry alle 5 s

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
  - CR2032-Alters-Anzeige: zeigt Monate seit dem letzten Batterietausch (Grün <24 Monate, Rot ab 24 = Tausch-Empfehlung, Gelb = Einbaudatum unbekannt). Button „Batterie heute getauscht" setzt das Einbaudatum neu (im EEPROM gespeichert). Hinweis: Eine echte Spannungsmessung ist ohne Zusatzhardware nicht möglich — die Anzeige basiert bewusst nur auf dem Alter
  - **Event-Log-Box**: Die letzten 10 Einträge (Fehler + Ereignisse wie Boot, Tageswechsel, Motor-Timeout, RTC-Probleme, nachgeholte Fütterungen) mit Zeitstempel direkt auf der Startseite – Fehlersuche ohne seriellen Monitor
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

⚠️ **Sicherheit**: Das Standard-Passwort ist im Quellcode und in der README öffentlich. Jeder im Funkbereich, der sich mit dem WLAN verbindet, kann füttern und Einstellungen ändern. Für den realen Einsatz unbedingt ein eigenes starkes Passwort in `setup()` setzen.

Wo ändern?
- Datei: `KORN_Rev3.1/Script_KORN_REV3.1/main/main.ino`
- In `setup()` wird der Access Point gestartet:

```cpp
// Stelle hier SSID/Passwort ein
apInit("KORN", "Chaosfeeder");
```

Hinweise:
- Nach Änderung neu kompilieren/flashen.
- SSID/PW auch in der seriellen Startmeldung ausgegeben (zum schnellen Verbinden).
- WLAN-Sicherheit: WPA2-PSK. __Kein zusätzlicher Web-Login__ auf der Seite nötig; die Zugangsdaten gelten nur für das WLAN.

### 🧰 Standardwerte
* Fütterungszeit 1: 07:01
* Fütterungszeit 2: 16:01 (aktiv)
* Standard-Laufzeit: 5 s (erweitert auf 1–600 s = bis zu 10 Minuten für große Hühnerscharen)
* Blockadelöser (UI-Default): 2 s (nicht persistent, nur Eingabewert in der Seite)
* Mindestabstand zwischen Fütterungen: 2 Minuten (Sicherheitsfeature)

## 💾 Persistenz & Batterie

* Konfiguration wird im **EEPROM des Arduino** gespeichert und übersteht Stromausfälle (die DS3231 besitzt kein nutzbares RAM).
* Sind die EEPROM-Daten korrupt oder nicht vorhanden, lädt das Gerät **Werkseinstellungen** (Defaults).
* **CR2032-Alters-Tracker**: Das Einbaudatum der Pufferbatterie wird im EEPROM gespeichert (Web-Button „Batterie heute getauscht"). Die Web-UI zeigt das Alter in Monaten; ab 24 Monaten wird der Tausch empfohlen. Eine CR2032 hält im DS3231 typischerweise 2–5 Jahre (theoretisch ~8 Jahre bei ~3 µA) — der 24-Monats-Rhythmus ist eine konservative Empfehlung für den Feldeinsatz.
* RTC-Stromverlust-Ereignisse (`lostPower()`) werden im Event-Log auf der Startseite angezeigt — ein Hinweis darauf, dass die Batterie einen stromlosen Zeitraum nicht überbrückt hat (Batterie schwach oder Kontaktproblem).
* Indikatoren in der seriellen Ausgabe: `RTC: OK/--`, `CFG: EEPROM/DEF`.

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
• **RTC-Status**: Ob die DS3231-Uhr erreichbar ist und eine gültige Zeit liefert (`RTC:OK`)
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
- Uhr (RTC) erreichbar und Zeit gültig
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
* **Rote Alters-Anzeige (≥24 Monate)**: CR2032 tauschen und danach den Button „Batterie heute getauscht" drücken
* **Gelbe Anzeige (Einbaudatum unbekannt)**: Beim nächsten Batterietausch (oder jetzt, wenn das Einbaudatum bekannt ist) den Button „Batterie heute getauscht" drücken
* **Log-Eintrag „RTC Stromverlust"**: Die Uhr ist während eines Stromausfalls stehen geblieben → Batterie leer oder Kontaktproblem. Test: Gerät 5 Minuten komplett stromlos machen; zeigt die Uhr danach die falsche Zeit, Batterie tauschen
* **Einstellungen gehen verloren**: Die Konfiguration liegt im EEPROM und ist batterieunabhängig; bei Verlust EEPROM-Fehler prüfen (Event-Log)
* ⚠️ Stirbt die CR2032 auffällig schnell (Wochen/Monate), hat das DS3231-Modul vermutlich eine aktive Ladeschaltung → Diode auslöten oder LIR2032 verwenden (siehe Verdrahtungshinweise)

### Notfall-Fütterung (Hardware-Taster)
Falls die Webseite nicht funktioniert: Pin 10 und Pin 11 am Arduino kurz mit einem Draht verbinden → Sofortige Fütterung wird ausgelöst (umgeht 2-Minuten-Regel).

---

## ⚡ Stromverbrauch & Solarbetrieb

Für den autarken Off-Grid-Betrieb mit Solarstrom sind folgende Verbrauchswerte relevant:

### Gemessene Stromaufnahme bei 12V
- **Standby** (Motor aus, Relais aus, Arduino an): **0,093 A** (1,116 W)
- **Aktiv ohne Last** (Motor läuft, DM320T aktiv, ohne Futter): **0,86 A** (10,32 W)
- **Aktiv mit Last** (Motor läuft, DM320T aktiv, mit Futter): **1,3 A** (15,6 W)

### DM320T Konfiguration (aktuell)
Die Messwerte basieren auf folgender DIP-Schalter-Einstellung der Variante A:
- SW1=ON, SW2=ON, SW3=OFF, SW4=ON, SW5=ON, SW6=ON
- Entspricht: 1,3A Peak (0,92A RMS), Microstep 2

### Dimensionierung für Solarbetrieb

**Täglicher Energiebedarf (Beispielrechnung bei 3 Fütterungen à 10s MIT Futter):**
- Standby: 23,992h × 0,093A = 2,231 Ah/Tag (26,8 Wh/Tag)
- Fütterungen: 3× 10s × 1,3A = 0,011 Ah/Tag (0,13 Wh/Tag)
- **Gesamt: ~2,24 Ah/Tag (26,9 Wh/Tag)**

**Hinweis:** Standby-Verbrauch dominiert (>99%)! Selbst mit Futterlast spielt die Fütterungszeit kaum eine Rolle.

**Empfohlene Solaranlage:**
- Solarpanel: 20-30W (je nach Standort/Jahreszeit)
- Akku: 12V/7-12Ah (84-144 Wh Kapazität)
- Laderegler: 12V PWM/MPPT für entsprechende Panel-Leistung

**Beispiel-Konfiguration (getestet):**
- Solarpanel: 130W (deutlich überdimensioniert → sehr zuverlässig)
- Akku: 12V/12Ah Sealed Lead Acid (144 Wh nominal, ~72 Wh nutzbar)
- Laderegler: entsprechend 130W Panel dimensioniert

### Batterielaufzeit ohne Solar-Nachladung

**12V 12Ah Blei-Akku (SLA/AGM):**
- Nutzbare Kapazität: ~50% (um Lebensdauer zu erhalten) = 6 Ah
- Bei 3 Fütterungen/Tag (à 10s): **2,7 Tage** (≈ 65 Stunden)
- Bei längeren Fütterungen (3× 60s): **2,6 Tage** (≈ 62 Stunden)

**Wichtig:** Die Fütterungsdauer hat kaum Einfluss auf die Gesamtlaufzeit, da der Standby-Verbrauch dominiert (>99% der Energie).

**Hinweise:**
- Blei-Akkus: Nicht unter 50% Entladung betreiben (verkürzt Lebensdauer drastisch)
- Wintermonate: Größeres Panel oder zusätzliche Akkukapazität einplanen
- Sicherheitsreserve: Mind. 20% Buffer einrechnen
- Alternative: LiFePO4-Akkus bieten ~80% nutzbare Kapazität (≈4 Tage Laufzeit), sind aber teurer

---

## Entwicklung

Der Code wurde mit Hilfe von **künstlicher Intelligenz** entworfen, optimiert und systematisch verbessert. Das Projekt folgt **Open-Source-Prinzipien** und ist vollständig dokumentiert.

### 🗂️ Projektstruktur
```
KORN_Rev3.1/
├── Script_KORN_REV3.1/
│   └── main/
│       ├── main.ino
│       ├── homepage.ino
│       ├── ap.ino
│       └── motor.ino
├── pics_video_additional-info/
│   └── DM320T_user_manual.txt
└── readme_rev3.1.md
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
- In `KORN_Rev3.1/Script_KORN_REV3.1/main/motor.ino` bei dieser Verdrahtung typischerweise:
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
  - RTClib by Adafruit (Autor: Adafruit / Michael Hord)
    - Unterstützt DS3231-RTC über I2C (`RTC_DS3231`/RTClib)
  - WDT (im UNO-R4-Core enthalten, kein Download nötig)
    - Hardware-Watchdog (`#include <WDT.h>`)

Hinweise:
- Installation jeweils über „Werkzeuge → Bibliotheken verwalten…“, nach den oben genannten Namen suchen.
- Nach Installation Board „Arduino UNO R4 WiFi“ wählen und den richtigen COM/TTY-Port einstellen.
 - Falls AccelStepper nicht gelistet ist: ZIP installieren über „Sketch → Include Library → Add .ZIP Library…“
   - ZIP: https://github.com/airspayce/AccelStepper/archive/refs/heads/master.zip

---

**Status:** In Entwicklung | **Version:** 2025.8 | **Lizenz:** Open-Source CAD-Hardware/Software
