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

## 🚀 Software-Features

* Eigenes WLAN (Access Point): SSID "KORN", Passwort "Chaosfeeder"
  Eigenes PW kann später im Script gesetzt werden!!
* Einfache Handy-Webseite (mobilfreundlich):
  - Zwei Fütterungszeiten einstellen
  - Zweite Fütterungszeit deaktivieren
  - MOTOR_SCHRITTE festlegen
  - Countdown bis zur nächsten Fütterung
  - Button "Jetzt füttern"
* Manuelle Bedienung:
  - Kurzschluss Pin 10 ↔ 11 als Taster
  - Sofortige Fütterung unabhängig vom Zeitplan
* Uhrzeit-/Zeitzonen-Handling ohne Internet: Beim Upload wird die RTC auf die lokale PC-Zeit gesetzt (keine automatische Sommer-/Winterzeit-Umstellung)
* Tages-Reset um Mitternacht
* Stepper-Reset nach jeder Bewegung

> Hinweis: AP-Insellösung (Off-Grid)
> - KORN stellt ein eigenes WLAN bereit und nutzt im AP-Modus fest die IP 192.168.4.1/24.
> - Für die Nutzung einfach mit dem WLAN "KORN" verbinden und im Browser `http://192.168.4.1` öffnen.
> - Währenddessen besteht in der Regel keine Internetverbindung; das vermeidet Konflikte mit Heimnetzwerken.

### 🧰 Standardwerte
* Fütterungszeit 1: 07:01
* Fütterungszeit 2: 16:01 (aktiv)
* MOTOR_SCHRITTE: 2000

---

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
├── main/
│   └── KORN-Motorsteuerung_Rev3/
│       └── KORN-Motorsteuerung_REV3.ino
├── pics_video_additional-info/
│   └── DM320T_user_manual.txt
└── README.md
```

---

**Status:** In Entwicklung | **Version:** 2025.8 | **Lizenz:** Open-Source CAD-Hardware/Software
