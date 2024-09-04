// Bibliotheken einbinden
#include <Ethernet.h>
#include <SPI.h>
#include "Configuration.h"

// Konstanten für switch/case
#define STATE_IDLE                0
#define STATE_SEND_HELLO          1
#define STATE_WAITING_FOR_ACK     2
#define STATE_NEEDED_DATA         3
#define STATE_NEEDED_DATA_ACK     4
#define STATE_NEEDED_DATA_END     5
#define STATE_NEEDED_DATA_END_ACK 6
#define STATE_RECIEVING_COMMANDS  7
#define STATE_RECIEVING_DATA      8

// Variable für switch/case
byte state;

// Betriebsart des Shields als Client
EthernetClient client;

// <-- Anfang des 1. Bereichs für individuelle Anpassungen -->
// Pinbelegungen für angeschlossene LED
int ledPZB40 = 2;
int ledPZB55 = 3;
int ledPZB500 = 5;
int ledPZB70 = 6;
int ledPZB85 = 7;
int ledPZB1000 = 8;
// ACHTUNG: je nach Arduino stehen einige PINs hier NICHT für
// Ausgaben frei zur Verfügung, da möglicherweise intern belegt. 
// Darunter fallen ggf. PIN 0, 1, 4, 10, 50, 51, 52, 53


void setup()
{
// PINs für Ausgaben als Ausgänge setzen
pinMode(ledPZB55, OUTPUT);
pinMode(ledPZB70, OUTPUT);
pinMode(ledPZB85, OUTPUT);
pinMode(ledPZB40, OUTPUT);
pinMode(ledPZB500, OUTPUT);
pinMode(ledPZB1000, OUTPUT);
// <-- Ende des 1. Bereichs für individuelle Anpassungen -->


Serial.begin(9600);
if (dhcp == 1)
  { 
  Serial.println("-> DCHP-Server connecting...");
  Ethernet.begin(mac);
  }
else
  {
  Serial.println("-> Server connecting...");
  Ethernet.begin(mac, shield_ip);
  }

delay(1000);

// Ausgabe ob Verbindung erstellt
if (client.connect(server_ip, port))
  { Serial.println("connected"); }
else
  { Serial.println("connection failed"); }
// zu case1
state = STATE_SEND_HELLO;
}


void loop()
{
switch(state)
  {

  case STATE_IDLE:
  break;

  // Anmeldung mit Name und Clienttype am Server
  case STATE_SEND_HELLO:
  #define NUM_SEND_HELLO 27
  byte hello[NUM_SEND_HELLO];
  hello[0] = 0x17; // PACKET_LENGTH (4 bytes)

  hello[1] = 0x00;
  hello[2] = 0x00;
  hello[3] = 0x00;
  hello[4] = 0x00; // HELLO (2 bytes)

  hello[5] = 0x01;
  hello[6] = 0x01; // VERSION (1 byte)

  hello[7] = 0x02; // CLIENT_TYPE (1 byte)
  hello[8] = 0x12; // IDENTIFICATION LENGTH
  hello[9] = 0x46; // "F"
  hello[10] = 0x61; // "a"
  hello[11] = 0x68; // "h"
  hello[12] = 0x72; // "r"
  hello[13] = 0x70; // "p"
  hello[14] = 0x75; // "u"
  hello[15] = 0x6C; // "l"
  hello[16] = 0x74; // "t"
  hello[17] = 0x20; // " "
  hello[18] = 0x28; // "("
  hello[19] = 0x41; // "A"
  hello[20] = 0x72; // "r"
  hello[21] = 0x64; // "d"
  hello[22] = 0x75; // "u"
  hello[23] = 0x69; // "i"
  hello[24] = 0x6E; // "n"
  hello[25] = 0x6F; // "o"
  hello[26] = 0x29; // ")"
  client.write(hello, NUM_SEND_HELLO);
  state = STATE_WAITING_FOR_ACK;
  break;


  case STATE_WAITING_FOR_ACK:
  if (client.available()) { state = STATE_NEEDED_DATA; }
  // falls keine Verbindung möglich
  notConnected();
  break;
  

  // <-- Anfang des 2. Bereichs für individuelle Anpassungen -->
  // Hier Anforderung der benötigten IDs
  case STATE_NEEDED_DATA: {
  #define NUM_NEEDED 15  // Gesamte Größe von needed, Feld 0 mitzählen!
  byte length = NUM_NEEDED-4;
  byte needed[NUM_NEEDED];
  needed[0] = length; // PACKET_LENGTH (4 bytes)
  needed[1] = 0x00;
  needed[2] = 0x00;
  needed[3] = 0x00;
  needed[4] = 0x00; // NEEDED DATA (2 bytes)

  needed[5] = 0x03;
  needed[6] = 0x00; // Befehlsvorrat (2 bytes)
  needed[7] = 0x0A; 
  // ab dieser Stelle können eigene Befehle ergänzt werden
  // Übersicht der Befehle unter Configuration.h
  needed[8] = 20; // PZB "1000"
  needed[9] = 21; // PZB "500"
  needed[10] = 22; // PZB "40"
  needed[11] = 23; // PZB "55"
  needed[12] = 24; // PZB "70"
  needed[13] = 25; // PZB "85"
  needed[14] = 86; // "Türen"
  client.write(needed, NUM_NEEDED);
  state = STATE_NEEDED_DATA_ACK;
  break; }
  // <-- Ende des 2. Bereichs für individuelle Anpassungen -->


  case STATE_NEEDED_DATA_ACK:
  if (client.available()) { state = STATE_NEEDED_DATA_END; }
  // falls keine Verbindung möglich
  notConnected();
  break;


  case STATE_NEEDED_DATA_END:
  byte data_end[8];
  data_end[0] = 0x04; // PACKET_LENGTH (4 bytes)
  data_end[1] = 0x00;
  data_end[2] = 0x00;
  data_end[3] = 0x00;
  data_end[4] = 0x00; // NEEDED DATA Letzter Befehl (2 bytes)
  data_end[5] = 0x03;
  data_end[6] = 0x00; // Befehlsvorrat (2 bytes)
  data_end[7] = 0x00;
  client.write(data_end, 8);
  state = STATE_NEEDED_DATA_END_ACK;
  break;


  case STATE_NEEDED_DATA_END_ACK:
  if (client.available()) { state = STATE_RECIEVING_COMMANDS; }
  // falls keine Verbindung möglich
  notConnected();
  break;


  // erster Durchlauf nur mit best. der IDs
  case STATE_RECIEVING_COMMANDS:
  if (client.available())
    {
    // Auslesen der ersten 4 Bytes (Paketlänge)
    char c[4];
    for(int i=0; i<4; i++) { c[i] = client.read(); }
    // Umrechnen der 4 Byte in Wert
    unsigned long *lval = (unsigned long *)c;
    unsigned long schleife = *lval;
    // Auslesen des Paketinhaltes
    char d[schleife];
    for(int i=0; i<schleife; i++) { d[i] = client.read(); }
    state = STATE_RECIEVING_DATA;  
    }
  // falls keine Verbindung möglich
  notConnected();
  break;


  // ab hier regulärer Empfang der Daten
  case STATE_RECIEVING_DATA:
  if (client.available())
    {
    // Auslesen der ersten 4 Bytes (Paketlänge)
    char c[4];
    for(int i=0; i<4; i++) { c[i] = client.read(); }
    // Umrechnen der 4 Bytes in Werte
    unsigned long *lval = (unsigned long *)c; //unsigned long umfässt 4 bytes
    unsigned long schleife = *lval;
    // Auslesen des Paketinhaltes
    char d[schleife];
    for(int i=0; i<schleife; i++) { d[i] = client.read(); }
    // Berechnen, wieviele Befehle übermittelt wurden
    int befehle = (schleife - 2) / 5;
    // Ausgabe nur wenn Werte verändert
    if(schleife > 2)
      {
      int size = sizeof(d);
      if(size > 3)
        {
        int x = 2;
        int y = 3;
        for(int i=0; i<befehle; i++)
          {
          Serial.print("Befehl: ");
          Serial.print( );
          Serial.print(" Wert: ");

          // single-Werte aus dem Bitmuster lesen
          float singlewert = *((float *)&d[y]);
          // enum (int)-Werte aus dem Bitmuster lesen
          int intwert = *((int *)&d[y]);
          //y += 4;

          // <-- Anfang des 3. Bereichs für individuelle Anpassungen -->
          switch(d[x])
            {
            case 20:
            Serial.println(singlewert);
            digitalWrite(ledPZB1000, singlewert != 0);
            break;
            
            case 21:
            Serial.println(singlewert);
            digitalWrite(ledPZB500, singlewert != 0);
            break;
            
            case 22:
            Serial.println(singlewert);
            digitalWrite(ledPZB40, singlewert != 0); 
            break;
            
            case 23:
            Serial.println(singlewert);
            digitalWrite(ledPZB55, singlewert != 0); 
            break;
            
            case 24:
            Serial.println(singlewert);
            digitalWrite(ledPZB70, singlewert != 0); 
            break;
            
            case 25:
            Serial.println(singlewert);
            digitalWrite(ledPZB85, singlewert != 0); 
            break;
            
            case 86:
            Serial.print(intwert);
            if(intwert == 0) { Serial.println(" (Tueren freigegeben)"); }
            if(intwert == 1) { Serial.println(" (Tueren offen)"); }
            if(intwert == 2) { Serial.println(" (Achtungspfiff oder Durchsage)"); }
            if(intwert == 3) { Serial.println(" (Fahrgaeste i. O.)"); }
            if(intwert == 4) { Serial.println(" (Tueren schliessen)"); }
            if(intwert == 5) { Serial.println(" (Tueren zu)"); }
            if(intwert == 6) { Serial.println(" (Abfahrauftrag Zp9)"); }
            if(intwert == 7) { Serial.println(" (Tueren verriegelt)"); }
            if(intwert == 8) { Serial.println(" (Ak. Freigabesignal)"); }
            break;
            }
            // Ende des 3. Bereichs für individuelle Anpassungen -->
          x += 5;
          y += 5;
          }
        Serial.println();  
        }
      else Serial.println();
      }
    }
  // falls keine Verbindung möglich
  notConnected();
  break;

  } // Ende switch/case

} // Ende void loop


void notConnected()
{
  if(!client.connected())
    {
    Serial.println();
    Serial.println("disconnecting.");
    client.stop();
    for(;;)
    ;
    }
}
