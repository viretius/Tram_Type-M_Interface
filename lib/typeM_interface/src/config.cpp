#include "config.h"                     

uint8_t VERBOSE;    
bool config_menu;
bool run_config_task;

Preferences preferences;

MCP_Struct mcp_list[MAX_IC_COUNT]; 
PCF_Struct pcf_list[MAX_IC_COUNT];

uint8_t acceleration_button[2]; 
uint8_t deceleration_button[2];
uint8_t combined_throttle_ic[2];

TaskHandle_t Task1;   //digital input
TaskHandle_t Task2;   //analog input
TaskHandle_t Task3;   //output
TaskHandle_t Task4;   //serial rx / tcp rx
TaskHandle_t Task5;   //serial tx 
TaskHandle_t Task6;   //config menu

QueueHandle_t serial_tx_info_queue;
QueueHandle_t serial_tx_verbose_queue;

SemaphoreHandle_t i2c_mutex;

byte mac[6]; 
IPAddress host;                 
//extern IPAddress client_ip;         
uint16_t port; 
 
byte handshakedata[] = {
                        0x17, //packet length
                        0x00,
                        0x00,
                        0x00,
                        0x00,
                            
                        0x01,
                        0x01,

                        0x02,   // CLIENT_TYPE (1 byte) = Fahrpult
                        0x12,   //ident length
                                        //"Fahrpult (ESP32S3"
                        0x46,
                        0x61,
                        0x68,
                        0x72,
                        0x70,
                        0x75,
                        0x6c,   
                        0x74,
                        0x20, 
                        0x28, //"("

                        0x45,
                        0x53,
                        0x50,
                        0x33,
                        0x32,
                        0x53,
                        0x33,
                        0x29 //")"    
                    };          //data to be sent to server after connection is established 
size_t handshakedata_len = sizeof(handshakedata);

byte request[] = {
                    REQUEST_LEN - 4,
                    0x00,
                    0x00,
                    0x00,
                    0x00, 

                    0x03,
                    0x00, // Befehlsvorrat (2 bytes)
                    0x0A,

                    //request data
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
