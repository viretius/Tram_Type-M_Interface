#include "globals.h"      

//extern variables declared in header are initialized here

uint8_t VERBOSE;    
bool run_config_task;

Preferences preferences;

MCP_Struct mcp_list[MAX_IC_COUNT]; 
PCF_Struct pcf_list[MAX_IC_COUNT];

bool acceleration_button_status = false;
bool deceleration_button_status = false;
uint8_t acceleration_button[2]; 
uint8_t deceleration_button[2];
uint8_t combined_throttle_ic[2];

TaskHandle_t Task1;   //digital input
TaskHandle_t Task2;   //analog input
TaskHandle_t Task3;   //output
TaskHandle_t Task4;   //serial rx / tcp rx
TaskHandle_t Task5;   //serial tx 
TaskHandle_t Task6;   //config menu

QueueHandle_t serial_tx_verbose_queue;

SemaphoreHandle_t i2c_mutex;

byte mac[6]; 
IPAddress host;                 
uint16_t port; 

//server expects LSB first


byte handshake[] = { 0x0, 0x0, 0x0, 0x0, 
                        0x1, 0x0, 0x0, 0x0, 
                        0x0, 0x0, 0x1, 0x0,
                        0x4, 0x0, 0x0, 0x0, 
                        0x1, 0x0, 0x2, 0x0, 
                        0x4, 0x0, 0x0, 0x0, 
                        0x2, 0x0, 0x2, 0x0, 
                        0xA, 0x0, 0x0, 0x0, 0x3, 0x0, 
                        0x46, 0x61, 0x68, 0x72, 0x70, 0x75, 0x6C, 0x74, //"Fahrpult"
                        0x5, 0x0, 0x0, 0x0, 0x4, 0x0, 
                        0x32, 0x2E, 0x30,              //"2.0"
                        0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                        0xFF, 0xFF, 0xFF
                        };


/*
byte handshake[] = {
                        0x17, //packet length
                        0x00,
                        0x00,
                        0x00,
                        0x00,
                            
                        0x01,
                        0x01,

                        0x02,   // CLIENT_TYPE (1 byte) = Fahrpult
                        0x12,   //ident length
                                        
                        'F','a','h','r','p','u','l','t',0x20,'(','E','S','P','3','2','-','S','3',')'
                    };          //data to be sent to server after connection is established 
*/
size_t handshake_len = sizeof(handshake);

/*
byte request[] = {
                    REQUEST_LEN - 4,
                    0x00,
                    0x00,
                    0x00,
                    0x00, 

                    0x03,
                    0x00, // Befehlsvorrat (2 bytes) ???
                    0x0A,

                    //requested data
                    GESCHWINDIGKEIT,
                    SPANNUNG,
                    AFB_SOLL_GESCHWINDIGKEIT,
                    MG_BREMSE,
                    H_BREMSE,
                    R_BREMSE,
                    SCHNELLBREMSUNG,
                    NOTBREMSUNG,
                    AFB_GESCHWINDIGKEIT,
                    PZB_WACHSAM,
                    PZB_FREI,
                    PZB_BEFEHL,
                    SIFA,
                    HAUPTSCHALTER,
                    MOTOR_SCHALTER,
                    SCHALTER_FAHRTRICHTUNG,
                    PFEIFE,
                    SANDEN,
                    TUEREN
                };
*/

byte request[] = {   0x0, 0x0, 0x0, 0x0, 0x2, 0x0, 0x0, 0x0, 0x0, 0x0, 0x3, 0x0, 0x0, 0x0, 0x0, 0x0, 0xA, 0x0,
                     0x4, 0x0, 0x0, 0x0, 0x1, 0x0, 0x1, 0x0,  // Geschwindigkeit m/s
                     0x4, 0x0, 0x0, 0x0, 0x1, 0x0, 0x2, 0x0,  // Druck Hauptluftleitung
                     0x4, 0x0, 0x0, 0x0, 0x1, 0x0, 0x3, 0x0,  // Druck Bremszylinder
                     0x4, 0x0, 0x0, 0x0, 0x1, 0x0, 0x4, 0x0,  // Druck Hauptluftbehälter
                     0x4, 0x0, 0x0, 0x0, 0x1, 0x0, 0x1D, 0x0, // MG-Bremse
                     0x4, 0x0, 0x0, 0x0, 0x1, 0x0, 0x25, 0x0, // Fahrtrichtung Vorne
                     0x4, 0x0, 0x0, 0x0, 0x1, 0x0, 0x26, 0x0, // Fahrtrichtung Rueck
                     0x4, 0x0, 0x0, 0x0, 0x1, 0x0, 0x57, 0x0, // Zustand Federspeicherbremse
                     0x4, 0x0, 0x0, 0x0, 0x1, 0x0, 0x9, 0x0,  // Zugkraft gesamt
                     0x4, 0x0, 0x0, 0x0, 0x1, 0x0, 0x13, 0x0, // Hauptschalter
                     0x4, 0x0, 0x0, 0x0, 0x1, 0x0, 0x15, 0x0, // Fahrstufe
                     0x4, 0x0, 0x0, 0x0, 0x1, 0x0, 0x20, 0x0, // LM Hochabbremsung Aus/Ein
                     0x4, 0x0, 0x0, 0x0, 0x1, 0x0, 0x1b, 0x0, // LM Schleudern
                     0x4, 0x0, 0x0, 0x0, 0x1, 0x0, 0x1c, 0x0, // LM Gleiten
                     0x4, 0x0, 0x0, 0x0, 0x1, 0x0, 0x64, 0x0, // SIFA
                     0x4, 0x0, 0x0, 0x0, 0x1, 0x0, 0x65, 0x0, // Zugsicherung
                     0x4, 0x0, 0x0, 0x0, 0x1, 0x0, 0x66, 0x0, // Türen           <<<<<  T Ü R E N  >>>>>
                     0x4, 0x0, 0x0, 0x0, 0x1, 0x0, 0x7c, 0x0, // Zugkraft gesamt Steuerwagen
                     0x4, 0x0, 0x0, 0x0, 0x1, 0x0, 0x85, 0x0, // Fahrstufe Steuerwagen
                     0xFF, 0xFF, 0xFF, 0xFF,
                     0xFF, 0xFF, 0xFF, 0xFF,
                     0xFF, 0xFF, 0xFF, 0xFF};
byte request_end[] = {
                    0x04, // PACKET_LENGTH (4 bytes)
                    0x00,
                    0x00,
                    0x00,
                    0x00, 
                    0x03,
                    0x00, // Befehlsvorrat (2 bytes)
                    0x00
                };

size_t request_end_len = sizeof(request_end);

//  neopixelWrite(RGB_BUILTIN, RGB_BRIGHTNESS, 0, 0);  // Red
//  neopixelWrite(RGB_BUILTIN, 0, RGB_BRIGHTNESS, 0);  // Green
//  neopixelWrite(RGB_BUILTIN, 0, 0, RGB_BRIGHTNESS);  // Blue
