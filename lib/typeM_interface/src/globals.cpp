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
                                        
                        'F','a','h','r','p','u','l','t', 0x20,'T','y','p', 0x20,'M','(','E','S','P','3','2','-','S','3',')'
                    };          //data to be sent to server after connection is established 

size_t handshake_len = sizeof(handshake);


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
