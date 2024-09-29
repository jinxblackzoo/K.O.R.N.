# K.O.R.N.
Katastrophal Organisierter Nahrungsmittel Spender

K.O.R.N. wurde geplant als robuster, Open-Source/Hardware, wasserdichter, mit einfachen Mitteln konstruierter und Mäusesicherer Fütterungsautomat für Geflügel. 

Aufgrund der enttäuschenden Erfahrung mit gekauften Fütterungsautomaten welche trotz der teils hohen Preise entweder nach drei Wochen defekt waren, oder ganze Mäusefamilien durchfütterten, musste eine Eigenkonstruktion her. Die Entscheidungsgrundlage für das gewählte System mit einer Förderschnecke in einem Rohr, basiert auf einer Recherche im Dubbel Ausgabe von 2001.

Es handelt es sich um einen Stetigförderer (Schnecke), der Schüttgut (Futter) aus einem Silo (KG-Rohr) in einen Auswurfschacht befördert. Die Abgabezeit und Dauer der Abgabe (Menge der Schritte des Steppers) ist einstellbar.Das Gehäuse besteht aus überall erhältlichen, robusten und günstigen HT-, bzw KG-Rohren. Diese sollen garantieren, dass das Futter vor Mäusen, Insekten und Feuchtigkeit geschützt ist. 
Die elektronischen Bauteile bestehen aus Bauteilen, welche leicht verfügbar und standardisiert sind. Ausserdem sind alle Datenblätter und Wirings leicht mit einer Online Suche auffindbar. Alle Bauteile können in diversen Onlineshops leicht und relativ kostengünstig erworben werden.

Ein Relay wurde dazwischengeschaltet um den Stepper in der inaktiven Zeit stromlos zu schalten.
Die Bauteile der Förderschnecke wurden mittels Freecad (LGPL2+, CC-BY-3.0) entworfen. 
Die Förderschnecke wurde steckbar entworfen. Somit ist es möglich, die Schnecke auch auf kleineren 3D-Druckern zu drucken.  

Der Code wurde mithilfe von codeium.com entworfen und manuell angepasst. Aufgrund der fehlenden Schnittstelle zu einem Zeitgeber (RTC oder Wlan) wurde der Code entworfen, dass das Arduino 10sec nach dem Power up das Relay anzieht. Dabei wird der Motortreiber mit Spannung versorgt. Kurz danach startet das Arduino Programm den Stepper mit folgenden Werten und anschließend alle 24h:

// Define variables for stepper parameters
int acceleration = 100; // Beschleunigung des Steppers in Steps pro Sekunde
int targetRPM = 100;    // Zielgeschwindigkeit in RPM
int stepsToRun = 6000;  // Menge der Steps, die der Stepper laufen soll. Eine komplette Runde sind 200 Steps.
const long powerOffDelay = 5000; // Verzögerung ab wann der Stepper nach jedem Lauf stromlos gemacht wird.
const int relayDelay = 2000; // Verzögerung in Milisekunden zwischen der Aktivierung des Relay und dem Start des Steppers
