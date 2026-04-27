# K.O.R.N.
**Katastrophal Organisierter Runder Nahrungsmittelspender**

K.O.R.N. ist ein robuster, Open-Source/Hardware, wasserdichter, mit einfachen Mitteln konstruierter und mäusesicherer Fütterungsautomat für Geflügel oder jede andere Art von Hausgetier 😉

## 🎯 Projektübersicht

Aufgrund der enttäuschenden Erfahrung mit gekauften Fütterungsautomaten welche trotz der teils hohen Preise entweder nach drei Wochen defekt waren, oder ganze Mäusefamilien durchfütterten, musste eine Eigenkonstruktion her. Die Entscheidungsgrundlage für das gewählte System mit einer Förderschnecke in einem Rohr, basiert auf einer Recherche in der Dubbel-Ausgabe von 2001.

Es handelt sich um einen **Stetigförderer (Schnecke)**, der Schüttgut (Futter) aus einem Silo (KG-Rohr) in einen Auswurfschacht befördert. Das Gehäuse besteht aus überall erhältlichen, robusten und günstigen HT- bzw. KG-Rohren.

Revision 3.5 basiert auf der bewährten Rev3-Hardware (Arduino UNO R4 WiFi) und bringt eine neue WLAN-Logik: Statt eines eigenen Access Points verbindet sich KORN mit einem vorhandenen Heimnetz. Die Zeitsteuerung erfolgt über NTP (Internetzeit) – die DS1302 RTC entfällt vollständig.

Beim ersten Start oder wenn kein WLAN erreichbar ist, öffnet der Arduino automatisch einen temporären Einrichtungs-AP (`KORN-Setup`, PW: `Chaosfeeder`). Damit kann man per Handy die WLAN-Zugangsdaten eingeben. Nach dem Speichern startet der Arduino neu und verbindet sich mit dem Heimnetz. Die IP-Adresse wird beim Start seriell ausgegeben.

## ⚡ Hardware-Komponenten

### Arduino Uno + Stepper-Treiber System
- **Arduino UNO R4 WiFi** (Mikrocontroller)
- ~~DS1302 RTC~~ – entfällt, Zeitsteuerung über NTP
- **NEMA Stepper Motor** Antrieb
- **DM320T Stepper-Motor Treiber** Motortreiber
- **Relais-Modul JQC3F oder ähnliches NO/COM/NC** (Stromversorgung Motor ein/aus)
- **Aktiver Buzzer** (Akustische Warnsignale)
- **Pushbutton momentarily** (Pin 10+11 Kurzschluss)

### 🔌 Verkabelung
```
Stepper-Treiber DM320T (Steuersignale):
├── PUL+    → Arduino Pin 2 (Schrittimpulse)
├── DIR+    → Arduino Pin 3 (Drehrichtung)
├── ENA+    → Arduino Pin 5 (Motor aktivieren)
└── OPTO    → Arduino 5V  (gemeinsamer Plus Optokoppler)

Stepper-Treiber DM320T (Leistung):
├── GND     → 12V Netzteil -
├── +Vdc    → Relais NO
├── A+      → NEMA Stepper Motor
├── A-      → NEMA Stepper Motor
├── B+      → NEMA Stepper Motor
└── B-      → NEMA Stepper Motor

Buzzer:
├── VCC     → Arduino 5V
├── GND     → Arduino GND
└── SIG     → Arduino Pin 6

Pushbutton:
└── Pin 10 ↔ Pin 11 (Kurzschluss = manuelle Fütterung)

Relais:
├── S       → Arduino Pin 8
├── +       → Arduino 5V
└── -       → Arduino GND

Relais Schaltkontakte:
├── NO      → DM320T +Vdc
├── COM     → 12V Netzteil + (Mittlerer Pin)
└── NC      → nicht belegt

```

#### Hinweise zur Verdrahtung
* **DS1302 entfällt in Rev3.5** – Pins 7, 9, 12 sind frei.
* **DM320T** (Common-Cathode):
  - PUL−, DIR−, ENA− an Arduino GND
  - OPTO an Arduino 5V (gemeinsamer Plus der Optokoppler)
* **Relais**: 
  - HW-482: Kann ACTIVE-HIGH oder ACTIVE-LOW sein (je nach Modul-Variante)
  - **Test:** LED am Modul leuchtet bei HIGH = ACTIVE-HIGH, bei LOW = ACTIVE-LOW
  - Im Code anpassbar: `RELAY_ACTIVE_HIGH` in `motor.ino` (Standard: `true`)
  - Relais trennt nur die Plusleitung: 12V+ → COM → NO → +Vdc des DM320T
* **Manueller Knopf** (Pin 10↔11): 
  - Pin 11 als permanentes LOW (OUTPUT)
  - Pin 10 als INPUT_PULLUP
  - **Fütterung**: Knopf 1–3 Sekunden drücken & loslassen
  - **Factory Reset**: Knopf 10 Sekunden halten (Buzzer warnt ab 5s)
  - **Lockout**: Während Fütterung + 2s danach wird der Knopf ignoriert
* **ENA-Logik**: ENA=HIGH aktiviert den Treiber; ENA=LOW deaktiviert

## 🚀 Software-Features

### ⏰ Intelligente Fütterungslogik
* **Automatischer Zeitplan**: Bis zu 2 Fütterungszeiten täglich
* **Sicherheitsabstand**: Mindestens 2 Minuten zwischen Fütterungen
* **Verzögerungsanzeige**: Bei zu häufigen Fütterungsversuchen wird die exakte Wartezeit angezeigt
* **Erweiterte Laufzeit**: 1–600 Sekunden (10 Minuten) für unterschiedliche Futtermengen
* **Live-Updates**: Webseite zeigt aktuelle Uhrzeit und letzte Fütterung in Echtzeit

### 💾 Konfigurationsspeicherung & Datensicherheit
* **EEPROM**: Konfiguration und WLAN-Zugangsdaten werden dauerhaft im EEPROM gespeichert
* **Automatisches Backup**: Bei jedem Speichervorgang wird EEPROM aktualisiert
* **Status-Anzeige**:
  - **CFG:EEPROM** = Aus EEPROM geladen (normal)
  - **CFG:DEF** = Standard-Werte (erster Start)
* **EEPROM-Lebensdauer**: ~100.000 Schreibzyklen (bei normaler Nutzung jahrelang haltbar)

### 📱 Benutzerfreundliche Bedienung

* **WLAN-Client-Modus**: KORN verbindet sich mit dem Heimnetz (Router)
* **Ersteinrichtungs-AP mit Captive Portal**: Bei erstem Start oder fehlendem WLAN öffnet sich `KORN-Setup` (PW: `Chaosfeeder`) → Browser öffnet sich **automatisch** mit der Einrichtungsseite (Captive Portal, wie bei Hotels/Cafés)
* **Hostname „KORN"**: Arduino meldet sich beim Router als **`KORN`** an (DHCP Option 12) – erscheint unter diesem Namen in der Router-Oberfläche. Bei vielen Routern (FritzBox, Speedport, OpenWRT) ist KORN direkt unter `http://korn` oder `http://korn.fritz.box` erreichbar.
* Einfache Handy-Webseite (mobilfreundlich):
  - Zwei Fütterungszeiten einstellen
  - Zweite Fütterungszeit deaktivieren
  - Motor-Laufzeit in Sekunden festlegen (1–600 Sekunden = bis zu 10 Minuten)
  - Countdown bis zur nächsten Fütterung mit Live-Uhr
  - Button "Jetzt füttern" mit 2-Minuten-Mindestabstand
  - Intelligente Verzögerungsanzeige: Zeigt exakte Uhrzeit bei Mindestabstand-Verzögerung
  - Letzte Fütterung wird live aktualisiert (Zeit + Quelle: Manuell/Web/Timer)
  - Warn-Button „Blockade lösen (Rechtslauf)“ mit Laufzeit-Eingabe (Standard 2 s; Begrenzung 1–60 s). Nur kurzfristig verwenden!
  - NTP-Status-Anzeige: Grün = synchronisiert, Rot = kein NTP-Sync
  - Robuste HTTP-Header: No-Cache, Connection: close, Sicherheits-Header
  - 303 Redirect nach Formularaktionen (verhindert doppeltes Absenden bei Reload)
  - Footer mit GitHub-Link und Build-Datum
* Manuelle Bedienung:
  - Knopf an Pin 10 ↔ 11 (1–3s drücken = Fütterung)
  - 10s halten = Factory Reset (Buzzer warnt ab 5s)
  - Sofortige Fütterung unabhängig vom Zeitplan
* Automatische Sommer-/Winterzeit (Europa/Berlin via NTP-Heuristik)
* NTP-Resync alle 60 Minuten
* Tages-Reset um Mitternacht
* Stepper-Reset nach jeder Bewegung

> Hinweis: WLAN-Client-Betrieb
> - KORN verbindet sich mit dem Heimnetz; die IP-Adresse wird beim Start seriell ausgegeben.
> - Webseite im Browser über die angezeigte IP öffnen (z.B. `http://192.168.1.42`).
> - Während der Motor läuft, bleibt die Seite im Lademodus – das ist normal. Sobald der Motor stoppt, lädt sie wieder.

#### Zugangsdaten Einrichtungs-AP
- Einrichtungs-AP SSID: `KORN-Setup`
- Einrichtungs-AP Passwort: `Chaosfeeder`
- Einrichtungsseite: `http://192.168.4.1/setup`

Wo ändern?
- Datei: `Script_KORN_REV3.5/main/wifi_client.ino`
- Konstanten am Dateianfang:

```cpp
static const char* SETUP_AP_SSID = "KORN-Setup";
static const char* SETUP_AP_PASS = "Chaosfeeder";
```

Hinweise:
- Heimnetz-WLAN-Zugangsdaten werden über die Einrichtungsseite gesetzt und im EEPROM gespeichert.
- **WPA2-PSK erforderlich**: Der UNO R4 WiFi unterstützt nur WPA2. WPA3-only oder WPA2/WPA3 mixed mode können Verbindungsprobleme verursachen. Im Router auf **WPA2 only** stellen (oder separates 2.4GHz-Gastnetz mit WPA2 einrichten).

#### 🔍 KORN im Heimnetz erreichen

KORN meldet sich beim Router mit dem Hostname **`KORN`** an. Je nach Router:

**Am einfachsten (FritzBox, Speedport, OpenWRT):**
- Browser öffnen → **`http://korn`** eingeben → fertig
- FritzBox alternativ: `http://korn.fritz.box`

**Falls das nicht klappt (UniFi, Vodafone, Billig-Router):**
- **Router-Oberfläche**: Verbundene Geräte → nach **„KORN"** suchen → IP notieren
- **Router-App**: Fritz!App, UniFi Network → DHCP-Liste → „KORN"
- **Serieller Monitor** (nur bei Entwicklung): Arduino IDE → 115200 Baud → IP wird beim Start ausgegeben

**Tipp für alle Nutzer**: Im Router eine **DHCP-Reservierung** für KORN einrichten → IP bleibt dauerhaft gleich → als Lesezeichen speichern.

#### 🔒 Verborgene SSIDs (Hidden Networks)
Funktioniert – einfach den exakten Netzwerknamen in das SSID-Feld der Einrichtungsseite eintippen (Groß-/Kleinschreibung beachten). Das Netzwerk muss nicht in einer Scan-Liste sichtbar sein.

### 🧰 Standardwerte
* Fütterungszeit 1: 07:01
* Fütterungszeit 2: 16:01 (aktiv)
* Standard-Laufzeit: 5 s (erweitert auf 1–600 s = bis zu 10 Minuten für große Hühnerscharen)
* Blockadelöser (UI-Default): 2 s (nicht persistent, nur Eingabewert in der Seite)
* Mindestabstand zwischen Fütterungen: 2 Minuten (Sicherheitsfeature)

## 💾 Persistenz

* Konfiguration und WLAN-Zugangsdaten werden im **EEPROM** gespeichert und überstehen Stromausfälle.
* Sind keine gültigen Daten im EEPROM, lädt das Gerät **Werkseinstellungen** (Defaults).
* Indikatoren in der seriellen Ausgabe: `NTP: OK/--`, `CFG: EEPROM/DEF`.

#### 🔄 Factory Reset (Werkseinstellungen wiederherstellen)
**Wichtig:** Ein normaler Sketch-Upload löscht **nichts** – WLAN-Zugangsdaten und Konfiguration bleiben im EEPROM erhalten!

Für einen vollständigen Reset gibt es zwei Möglichkeiten:

**Option A – Über die Webseite (einfach):**
- Auf der Hauptseite den Button **"🗑️ Auf Werkseinstellungen zurücksetzen"** unter dem NTP-Status klicken
- Arduino startet neu mit leerem EEPROM → Einrichtungs-AP öffnet sich automatisch

**Option B – Hardware-Recovery (ohne PC, bei vergessenem Admin-Passwort):**
- Den **manuellen Knopf (Pin 10 ↔ 11) gedrückt halten**
- Nach 5 Sekunden beginnt der Buzzer zu piepen (Vorwarnung)
- Knopf **weiter halten** bis insgesamt 10 Sekunden
- Bei 10s: Buzzer aus + 3 lange Bestätigungs-Piepser → EEPROM wurde gelöscht
- KORN startet automatisch neu im Einrichtungs-AP `KORN-Setup`
- **Abbruch möglich**: Knopf vor 10s wieder loslassen → nichts passiert

**Option C – Per Sketch (falls Hardware-Recovery nicht möglich):**
- Arduino IDE: **Datei → Beispiele → EEPROM → eeprom_clear** öffnen
- Auf den Arduino flashen, warten bis serieller Monitor "Done clearing EEPROM" anzeigt
- Dann wieder den originalen KORN-Sketch flashen

**Was bleibt erhalten (normaler Betrieb):**
- Stromlosigkeit, Reboots, WLAN-Verbindungsverluste → **EEPROM bleibt gespeichert**
- Sketch-Updates → **EEPROM bleibt gespeichert**

---

## 🖥️ Serieller Monitor

- Baudrate: **115200 Baud**
- Datenbits/Parität/Stoppbits: 8-N-1 (Standard)
- Zeilenende: No line ending (keine Eingaben erforderlich)
- Port: das ACM-Gerät des Boards (z. B. `/dev/ttyACM0`)

### Was zeigt der serielle Monitor?

Hauptsächlich für Entwickler und Fehlersuche. Er zeigt:

• **WLAN-Status**: Verbundenes Heimnetz (SSID) und IP-Adresse
• **Aktuelle Uhrzeit**: Datum und Uhrzeit der internen Uhr
• **Letzte Fütterung**: Wann zuletzt gefüttert wurde und wie lange der Motor lief
• **Nächste Fütterung**: Countdown bis zur nächsten automatischen Fütterung
• **NTP-Status**: Ob die Uhrzeit über NTP synchronisiert ist
• **Betriebszeit**: Wie lange das Gerät bereits läuft

**Beispiel einer Statuszeile:**
```
SSID:MeinHeimnetz IP:192.168.1.42 | TIME:18:48 | Last:17:10(5000) | Next1:19:00(T-00:12) Next2:-- [off] | Steps:5000 | NTP:OK CFG:EEPROM | Up:00:32
```

**Bedeutung:**
- Verbunden mit WLAN "MeinHeimnetz", IP 192.168.1.42
- Aktuelle Zeit: 18:48 Uhr (per NTP)
- Letzte Fütterung: um 17:10 Uhr
- Nächste Fütterung: um 19:00 Uhr (in 12 Minuten)
- NTP: synchronisiert
- Gerät läuft seit 32 Minuten

---

## 🛡️ 24/7-Betriebssicherheit

KORN Rev3.5 ist für unbeaufsichtigten Dauerbetrieb ausgelegt. Folgende Mechanismen schützen vor Ausfällen:

### ⏱️ Watchdog
- **Hardware-Watchdog (4s Timeout)** auf UNO R4 aktiviert (`WDT.h`)
- Wenn der Code irgendwo hängt > 4s → automatischer Neustart
- Wird in `loop()` und während Motorlauf periodisch zurückgesetzt
- **Aktivierung erst nach WLAN-/NTP-Init**, weil `WiFi.begin()` intern länger als 4s blockieren kann (Kommunikation mit ESP32-S3-WiFi-Coprozessor). Während Boot ist der Nutzer eh anwesend.

### 🔌 Stromausfall-Verhalten
- **EEPROM persistiert**: WLAN-Daten, Konfiguration, Admin-Passwort, **letzte Fütterung & Tagesmarker**
- **Schutz vor Doppel-Fütterung**: Nach Stromausfall weiß KORN noch, ob heute schon gefüttert wurde
- **Stepper sicher**: Bei Stromverlust stoppt Motor sofort, Position wird beim Boot auf 0 gesetzt
- **Relais fällt ab**: Stepper-Treiber erhält keine 12V mehr, kein wilder Motorlauf

### 🌐 Internet-Ausfall
- **NTP-Resync alle 60min**, falls Internet zurückkommt
- **Bei kurzzeitigem Ausfall (< 24h)**: Zeit läuft per `millis()` weiter, geplante Fütterungen funktionieren
- **WLAN-Reconnect alle 30s**: Bei vorhandenen Credentials wird **kein Setup-AP** automatisch geöffnet
- **Bei langen Ausfällen**: Manuelle Fütterung per Hardware-Taster möglich (unabhängig von Internet/WLAN)

### 🔄 Was passiert bei...
| Szenario | Verhalten |
|----------|-----------|
| **Stromausfall < 1 Min** | Reboot → EEPROM-Daten geladen → keine Doppel-Fütterung |
| **Stromausfall > 5 Min** | Reboot, NTP-Sync → evtl. verpasste Fütterung wird **NICHT** nachgeholt |
| **WLAN-Ausfall** | Geplante Fütterungen laufen weiter (Zeit per `millis()` Drift) |
| **Internet-Ausfall (DSL)** | Wie WLAN-Ausfall: NTP-Resync schlägt fehl, Zeit driftet |
| **Code-Hänger** | Watchdog löst Reboot nach 4s aus |
| **Router-Reboot** | Reconnect alle 30s, max. 3 Versuche im Vordergrund |

---

## 🐔 Häufige Fragen für Hühnerfreunde

> Einfache Antworten für alle, die kein IT-Studium haben.

### ❓ Was passiert, wenn der Strom kurz weg war?
**Nichts Schlimmes.** KORN startet neu und läuft normal weiter. Das Gerät merkt sich im internen Speicher, ob heute schon gefüttert wurde – **deine Hühner bekommen nicht zweimal Futter**. Die Uhrzeit wird automatisch aus dem Internet geholt (NTP), sobald das WLAN wieder da ist.

### ❓ Was ist, wenn ich das Admin-Passwort vergesse?
**Kein Problem – einfach den manuellen Knopf 10 Sekunden gedrückt halten:**
1. Knopf (Pin 10↔11) drücken und **halten**
2. Nach 5 Sekunden beginnt der Buzzer zu piepen (Vorwarnung)
3. Knopf weiter halten, bei 10 Sekunden hört das Piepen auf, dann kommen 3 lange Bestätigungs-Piepser
4. KORN startet automatisch neu im Einrichtungs-Modus (`KORN-Setup`, Passwort `Chaosfeeder`)

**Reset abbrechen:** Den Knopf während des Piepens (5-10s) wieder loslassen.

### ❓ Fallen meine Hühner vom Futter, wenn das Internet ausfällt?
**Für ein paar Stunden: nein.** KORN hat die Uhrzeit im Speicher und füttert weiter nach Plan. Bei langen Ausfällen (mehrere Tage) könnte die Zeit ein paar Minuten falsch laufen – aber gefüttert wird trotzdem. Notfalls kannst du immer den **manuellen Taster** drücken (funktioniert auch ohne Internet und WLAN).

### ❓ Der Router wurde neu gestartet – was nun?
**KORN verbindet sich automatisch wieder.** Alle 30 Sekunden wird geprüft, ob das Heimnetz zurück ist. Du musst nichts tun.

### ❓ Geht das Gerät kaputt, wenn es 24/7 läuft?
**Nein, es ist dafür gemacht.** Ein Hardware-Watchdog startet das Gerät automatisch neu, falls die Software mal hängen sollte (selten). Der Speicher ist robust genug für **über 100 Jahre** Dauerbetrieb mit zwei Fütterungen pro Tag.

### ❓ Kann meine Nachbarin an meine Hühnerfütterung?
**Nur wenn sie dein WLAN hat.** KORN ist **nicht aus dem Internet** erreichbar, nur aus deinem Heimnetz. Zusätzlich kannst du ein Admin-Passwort setzen, dann kommt auch niemand ohne dieses Passwort rein – selbst wenn jemand im WLAN ist.

### ❓ Muss KORN immer mit dem Computer verbunden sein?
**Nein.** Der USB-Anschluss ist nur für das erste Aufspielen der Software nötig. Danach braucht KORN nur einen Stromanschluss und WLAN.

### ❓ Wo sehe ich, ob KORN gerade läuft?
Rufe im Browser `http://korn` auf (oder die IP-Adresse). Dort siehst du:
- Aktuelle Uhrzeit
- Wann zuletzt gefüttert wurde
- Wann die nächste Fütterung kommt
- Status des Geräts

---

## 🔑 Admin-Passwort (optional)

Beim Einrichten kann ein **Admin-Passwort** (4-32 Zeichen) vergeben werden, um die Web-UI zu schützen.

**Verhalten:**
- **Leer gelassen** → KORN ist im Heimnetz frei zugänglich (jeder mit IP-Adresse)
- **Gesetzt** → Beim ersten Zugriff erscheint eine Login-Seite, das Passwort wird anschließend in der URL und in Formularen weitergereicht

**Geändert wird das Passwort durch:**
1. Factory Reset (Web-UI Button **oder** Hardware-Recovery)
2. Neue Einrichtung mit anderem Admin-Passwort

**Vergessen? → Hardware-Recovery:**
- Manuellen Knopf (Pin 10↔11) **10 Sekunden** gedrückt halten
- Buzzer piept ab 5s als Vorwarnung
- Bei 10s: 3 lange Piepser → EEPROM gelöscht, KORN startet automatisch im Setup-AP

**Sicherheitshinweise:**
- Passwort wird **unverschlüsselt** über HTTP übertragen → nur im vertrauenswürdigen Heimnetz nutzen
- Passwort wird im EEPROM **im Klartext** gespeichert
- Schützt nicht vor Netzwerk-Sniffing, aber vor versehentlichen Klicks und unautorisierten Nutzern im LAN

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

### Häufige Probleme und Lösungen

#### Webseite lädt nicht
* **IP unbekannt**: Serielle Ausgabe lesen (115200 Baud) – IP wird beim Start ausgegeben
* **Seite nicht erreichbar**: Sicherstellen, dass Handy/PC im gleichen Heimnetz ist

#### WLAN-Probleme
* **Arduino verbindet sich nicht / Timeout**: 
  - Einrichtungs-AP `KORN-Setup` öffnet sich automatisch → `http://192.168.4.1/setup` aufrufen
  - **WPA2-PSK prüfen**: Router muss WPA2-only unterstützen (WPA3 oder Mixed-Mode verursachen Timeouts)
  - SSID und Passwort exakt prüfen (Groß-/Kleinschreibung, keine Leerzeichen)
* **Kein NTP-Sync**: Internetverbindung des Routers prüfen

#### Fütterung funktioniert nicht
* **"Jetzt füttern" reagiert nicht**: 
  - 2 Minuten seit letzter Fütterung warten
  - Webseite zeigt dann exakte Wartezeit an
* **Motor dreht nicht**: 
  - 12V-Netzteil angeschlossen und eingeschaltet?
  - Alle Kabel fest verbunden?
  - Grünes Lämpchen am Arduino leuchtet?

#### NTP-Probleme
* **Roter NTP-Button**: Router hat keine Internetverbindung, oder NTP-Server nicht erreichbar
* **Einstellungen gehen verloren**: Nur bei EEPROM-Fehler (sehr selten) – siehe [Factory Reset](#-factory-reset-werkseinstellungen-wiederherstellen)

### Notfall-Fütterung (Hardware-Knopf)
Falls die Webseite nicht funktioniert: Knopf an Pin 10↔11 **1–3 Sekunden** drücken und loslassen → Sofortige Fütterung wird ausgelöst (umgeht 2-Minuten-Regel).

---

## 🔒 Sicherheit & Firewall

**KORN Rev3.5 ist ein IoT-Gerät ohne eingebaute Sicherheitsfeatures.** Diese Übersicht hilft bei der eigenen Risikobewertung. Jeder soll selbst entscheiden, ob und wie das Gerät eingesetzt wird.

### 🔴 Bekannte Risiken (kritisch)

| # | Risiko | Beschreibung | Warum vertretbar / nicht behoben |
|---|--------|--------------|----------------------------------|
| 1 | **HTTP statt HTTPS** | Webseite läuft auf Port 80 unverschlüsselt – Daten im LAN im Klartext sichtbar | Der Arduino UNO R4 WiFi unterstützt kein TLS als Server (nur als Client). HTTPS wäre technisch nicht umsetzbar ohne kompletten Hardware-Wechsel. Das einzige sensible ist das WLAN-Passwort bei der Ersteinrichtung – danach werden nur unkritische Fütterungsdaten übertragen. |
| 2 | **Optional: Login-Authentifizierung** | Web-UI kann durch ein Admin-Passwort geschützt werden (bei Einrichtung optional vergeben). Ohne Passwort hat jeder im Netzwerk vollen Zugriff. | Bei Einrichtung kann ein Admin-Passwort (4-32 Zeichen) gesetzt werden. Falls vergessen: Hardware-Recovery (Knopf Pin 10↔11 zur Laufzeit 10s halten) löscht das EEPROM. Ohne Admin-Passwort: Im privaten Heimnetz mit VLAN-Trennung tolerierbar. |
| 3 | **WLAN-Passwort im EEPROM (Klartext)** | Wer physischen Zugriff auf den Arduino hat, kann das WLAN-Passwort auslesen | Erfordert physischen Zugriff auf das Gerät + Arduino IDE + Spezialkenntnisse. Wer physischen Zugriff zum Hühnerstall hat, könnte das WLAN-Gerät ohnehin einfach mitnehmen. Arduino in abgeschlossenem Gehäuse minimiert dieses Risiko. |

### 🟡 Bekannte Risiken (mittel)

| # | Risiko | Beschreibung | Warum vertretbar / nicht behoben |
|---|--------|--------------|----------------------------------|
| 4 | **Kein Rate-Limit / DoS-Schutz** | Bei vielen Anfragen kann der Webserver überlastet werden | KORN ist nur im lokalen Netz erreichbar, nicht aus dem Internet. Ein gezielter DoS-Angriff aus dem eigenen Heimnetz ist kein realistisches Szenario. Worst Case: Webserver temporär nicht erreichbar, Fütterungslogik läuft weiter. **Watchdog (4s)** rettet bei Hängern. |
| 5 | **Einrichtungs-AP ohne Session-Schutz** | Im Setup-AP kann jeder (mit Passwort) WLAN-Daten ändern | Der Setup-AP ist mit WPA2 und Passwort (`Chaosfeeder`) gesichert. Nur wer das Passwort kennt, kommt rein. Setup-AP ist nur aktiv wenn kein WLAN verbunden – also nur kurzzeitig bei Ersteinrichtung oder nach Factory Reset. |
| 6 | **CSRF (Cross-Site Request Forgery)** | Bösartige Webseiten könnten Fütterung/Reset auslösen | Erfordert dass Nutzer gleichzeitig eine bösartige Webseite und die KORN-Seite offen hat, und ein Angreifer die interne KORN-IP kennt. Sehr unwahrscheinliches Szenario für ein Heimgerät. Worst Case: ungewollte Fütterung oder Reset. |
| 7 | **DNS-Server im Setup-Modus** | Captive Portal DNS-Server ist UDP-offen | DNS-Server läuft **ausschließlich** im Einrichtungs-AP-Modus und antwortet nur auf Anfragen im 192.168.4.x-Netz. Sobald KORN mit dem Heimnetz verbunden ist, wird der DNS-Server automatisch gestoppt. |

### 🟢 Bekannte Risiken (niedrig)

| # | Risiko | Beschreibung | Warum vertretbar / nicht behoben |
|---|--------|--------------|----------------------------------|
| 8 | **NTP-Outbound-Verkehr** | Arduino kontaktiert `pool.ntp.org` (UDP 123) | Standardverhalten aller NTP-Clients weltweit. Kein Angriffsvektor – es werden keine persönlichen Daten übertragen, nur ein Zeitstempel angefragt. Firewall-Regel erlaubt nur diesen einen Outbound-Port. |
| 9 | **Einrichtungs-AP öffnet sich automatisch** | Bei WLAN-Verlust wird der Setup-AP geöffnet | AP ist WPA2-gesichert mit Passwort. Öffnet sich nur wenn KORN kein Heimnetz erreicht (z.B. Router-Neustart). Wer das Setup-Passwort nicht kennt, kann nichts tun. |
| 10 | **Factory Reset ohne Passwort** | Jeder im Netz kann den Reset-Button drücken | Reset löscht nur EEPROM (WLAN-Zugangsdaten + Konfiguration) – keine persönlichen Daten, keine Dateien. Worst Case: KORN muss neu eingerichtet werden. Durch VLAN-Trennung hat niemand außer dem Admin Netzwerkzugriff auf das Gerät. |

### 🛡️ Firewall-Empfehlungen

- **VLAN-Trennung**: KORN in separates IoT-VLAN ohne Zugriff auf andere Netzwerkgeräte
- **Firewall-Regeln**: Nur NTP (UDP 123) erlauben, restlicher Internet-Verkehr blockieren
- **Port-Isolation**: LAN-Port auf IoT-VLAN taggen, falls per Kabel verbunden
- **Zugriffsbeschränkung**: HTTP-Port 80 nur für bestimmte Admin-IPs freigeben
- **WLAN-Settings**: „Client Isolation" im WLAN aktivieren (schützt andere Geräte)
- **Physischer Schutz**: Arduino in abgeschlossenem Gehäuse (schützt EEPROM-Zugriff)

### 📊 Empfehlung für private Nutzung
Für ein privates Setup (Hühnerstall, UniFi-Firewall mit VLAN-Trennung) ist das **Risiko vertretbar**:
- KORN nicht im gleichen VLAN wie sensible Geräte (NAS, Arbeits-PC)
- Firewall-Regeln begrenzen KORN auf das Minimum (NTP)
- Keine Weiterleitung von Ports nach außen (kein Internet-Zugriff auf KORN)

### ⚠️ Nicht empfohlen
- **Einsatz in fremden Netzen** (z.B. öffentliches WLAN)
- **Port-Weiterleitung aus dem Internet** auf KORN
- **Gemeinsames VLAN** mit sensiblen Geräten
- **Nutzung ohne Firewall-Konfiguration**

---

## ⚡ Stromverbrauch & Solarbetrieb

Für den Betrieb mit Solarstrom sind folgende Verbrauchswerte relevant. Hinweis: Rev3.5 benötigt eine Internetverbindung für NTP – ein WLAN-Router mit Internetzugang muss erreichbar sein:

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

## 🛠️ Entwicklung

Der Code wurde mit Hilfe von **künstlicher Intelligenz** entworfen, optimiert und systematisch verbessert. Das Projekt folgt **Open-Source-Prinzipien** und ist vollständig dokumentiert.

### 🗂️ Projektstruktur
```
KORN_Rev3.5/
├── Script_KORN_REV3.5/
│   └── main/
│       ├── main.ino          (Setup/Loop, NTP-Logik)
│       ├── wifi_client.ino   (WLAN-Client + Einrichtungs-AP)
│       ├── homepage.ino      (Webserver & Routen)
│       └── motor.ino         (Stepper, Relais, Buzzer)
├── cad/
├── pics_video_additional-info/
└── README_REV3.5.md
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
- In `Script_KORN_REV3.5/main/motor.ino` bei dieser Verdrahtung typischerweise:
  - `ENABLE_ACTIVE_HIGH = false`
  - `INVERT_STEP = true`
  - `INVERT_DIR` je nach Drehrichtung (falls invertiert → `true`)

### 📦 Arduino IDE: Benötigte Pakete & Bibliotheken

* __Board-Paket (Boards Manager)__
  - Arduino UNO R4 Boards (Renesas RA) – Board: „Arduino UNO R4 WiFi“ auswählen

* __Bibliotheken (Library Manager)__
  - AccelStepper (Autor: Mike McCauley / AirSpayce)
    - Stepper-Ansteuerung für den DM320T (`#include <AccelStepper.h>`)
  - WiFiS3 (vorinstalliert)
    - WLAN-Client und Webserver (`#include <WiFiS3.h>`)
  - WiFiUDP (vorinstalliert)
    - NTP-Kommunikation und DNS-Server für Captive Portal (`#include <WiFiUdp.h>`)
  - EEPROM (vorinstalliert)
    - Konfiguration und WLAN-Zugangsdaten speichern
  - ~~Rtc by Makuna~~ – entfällt (DS1302 nicht mehr verwendet)

Hinweise:
- Installation jeweils über „Werkzeuge → Bibliotheken verwalten…“, nach den oben genannten Namen suchen.
- Nach Installation Board „Arduino UNO R4 WiFi“ wählen und den richtigen COM/TTY-Port einstellen.
- Falls AccelStepper nicht gelistet ist: ZIP installieren über „Sketch → Include Library → Add .ZIP Library…“
  - ZIP: https://github.com/airspayce/AccelStepper/archive/refs/heads/master.zip

---

**Status:** In Entwicklung | **Version:** Rev3.5 (2026) | **Lizenz:** Open-Source CAD-Hardware/Software
