
// MAC - Adresse des Shields (z.B. auf Aufkleber auf dem Shield)
// wenn unbekannt, dann diese Werten lassen und damit probieren
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE,0xED};


// eigene Adresse des Zusi-Rechners mit dem TCP-Server
// die aktuell eingetragene Werte sind nur beispielhaft
byte server_ip[] = { 192, 168, 178, 33 };


// Port an dem der Zusi-Server läuft
// DEFAULT ist 1435
int port = 1435;


// <-- Ab hier sind  Änderungen nur bei Betrieb ohne DCHP nötig. Default ist MIT DHCP -->

// eigene Adresse für das Shield vergeben
// die aktuell eingetragene Werte sind nur beispielhaft
byte shield_ip[] = { 192, 168, 178, 177 };


// diesen Wert DHCP 0 setzen:
#define dhcp 1


/*
Nur zur Info an dieser Stelle: 
Liste der mit diesem Script übertragbaren Daten

Wert Typ     Bedeutung 
0            keine Funktion 
1    single  Geschwindigkeit 
2    single  Druck Hauptluftleitung 
3    single  Druck Bremszylinder 
4    single  Druck Hauptluftbehälter 
5    single  Zugkraft gesamt (nur ganzzahlige Werte)
6    single  Zugkraft pro Achse (nur ganzzahlige Werte)
7    single  Strom (nur ganzzahlige Werte)
8    single  Spannung (nur ganzzahlige Werte)
9    single  Motordrehzahl (nur ganzzahlige Werte)
10   single  Uhrzeit Stunde 
11   single  Uhrzeit Minute 
12   single  Uhrzeit Sekunde 
13   single  LZB Ziel-Geschwindigkeit 
14   single  LZB/AFB Soll-Geschwindigkeit 
15   single  LZB Zielweg 
16   single  Fahrstufe (nur ganzzahlige Werte) 
18   single  AFB Soll-Geschwindigkeit 
19   single  Druck Hilfsluftbehälter 
20   single  LM PZB 1000Hz 
21   single  LM PZB 500Hz 
22   single  LM PZB Befehl 
23   single  LM PZB Zugart U 
24   single  LM PZB Zugart M 
25   single  LM PZB Zugart O 
26   single  LM LZB H 
27   single  LM LZB G 
28   single  LM LZB E40 
29   single  LM LZB EL 
30   single  LM LZB Ende 
31   single  LM LZB V40 
32   single  LM LZB B 
33   siggle  LM LZB S 
34   single  LM LZB Ü 
35   single  LM LZB Prüfen 
36   single  LM Sifa 
37   single  LM Hauptschalter 
38   single  LM Getriebe 
39   single  LM Schleudern 
40   single  LM Gleiten 
41   single  LM Mg-Bremse 
42   single  LM H-Bremse 
43   single  LM R-Bremse 
44   single  LM Hochabbremsung 
45   single  LM Schnellbremsung 
46   single  LM Notbremsung 
47   single  LM Türen 
51   single  Schalter Fahrstufen (nur ganzzahlige Werte)
52   single  Schalter Führerbremsventil (nur ganzzahlige Werte)
53   single  Schalter dyn. Bremse (nur ganzzahlige Werte)
54   single  Schalter Zusatzbremse (nur ganzzahlige Werte)
55   single  Schalter AFB-Geschwindigkeit (nur ganzzahlige Werte)
56   single  Schalter AFB ein/aus (nur ganzzahlige Werte)
57   single  Schalter Mg-Bremse (nur ganzzahlige Werte)
58   single  Schalter PZB Wachsam (nur ganzzahlige Werte)
59   single  Schalter PZB Frei (nur ganzzahlige Werte)
60   single  Schalter PZB Befehl (nur ganzzahlige Werte)
61   single  Schalter Sifa (nur ganzzahlige Werte)
62   single  Schalter Hauptschalter (nur ganzzahlige Werte)
63   single  Schalter Motor ein/aus (nur ganzzahlige Werte)
64   single  Schalter Fahrtrichtung (nur ganzzahlige Werte)
65   single  Schalter Pfeife (nur ganzzahlige Werte)
66   single  Schalter Sanden (nur ganzzahlige Werte)
67   single  Schalter Türen (nur ganzzahlige Werte)
68   single  Schalter Glocke (nur ganzzahlige Werte)
69   single  Schalter Lokbremse entlüften (nur ganzzahlige Werte)
70   single  Schalter Schleuderschutzbremse (nur ganzzahlige Werte)
71   single  LM Drehzahlverstellung 
72   single  LM Fahrtrichtung vor 
73   single  LM Fahrtrichtung zurück 
74   single  Schalter Signum (nur ganzzahlige Werte)
75   single  LM LZB Zielweg (ab 0) (nur ganzzahlige Werte)
76   single  LZB Soll-Geschwindigkeit 
77   single  LM Block, bis zu dem die Strecke frei ist (String) 
78   single  Schalter Lüfter (nur ganzzahlige Werte)
79   single  LM GNT G 
80   single  LM GNT Ü 
81   single  LM GNT B 
82   single  LM GNT S  
85   single  Strecken-Km 
86   enum    Türen
87   enum    Autopilot 
88   enum    Reisezug 
89   enum    PZB-System 
90   single  Frames per Second 
91   enum    Führerstand sichtbar 
94   single  Bremshundertstel 
95   enum    Bremsstellung 
*/
