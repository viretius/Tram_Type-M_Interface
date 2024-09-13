#ifndef GLOBALS_H
#define GLOBALS_H

#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>

#include <Preferences.h> //for storing config in flash

#include <FS.h>
#include <LittleFS.h>
#include <CSV_Parser.h>

#include <USB.h>
#include <USBHIDKeyboard.h>

#include <Adafruit_PCF8591.h>                            
#include <MCP23017.h>       //slightly modded

//============================================================
//various global variables and definitions
//============================================================


#define MAX_IC_COUNT 8

#define ADC_STEP_THRESHOLD 5         //lower number results in higher sensitivity

#define DEBOUNCE_DELAY_MS 80         //debounce delay for digital inputs

#define CMD_BUFFER_SIZE 10          // "X"(start) "U"(D/A) "XX"(address) "XXXX"(data) "Y"(end) "\0" 
#define VERBOSE_BUFFER_SIZE 75         

#define MCP_I2C_BASE_ADDRESS 32     //0x20
#define PCF_I2C_BASE_ADDRESS 72     //0x48
#define MCP_I2C_END_ADDRESS (MCP_I2C_BASE_ADDRESS + MAX_IC_COUNT - 1)
#define PCF_I2C_END_ADDRESS (PCF_I2C_BASE_ADDRESS + MAX_IC_COUNT - 1)

#define CHECK_BIT(var, pos) ((var) & (1 << (pos))) 


//============================================================
//defines for requests to zusi/loksim3d-server 
//============================================================

#define GESCHWINDIGKEIT 1
#define SPANNUNG 8
#define AFB_SOLL_GESCHWINDIGKEIT 18
#define MG_BREMSE 41
#define H_BREMSE 42
#define R_BREMSE 43
#define SCHNELLBREMSUNG 45
#define NOTBREMSUNG 46
#define AFB_GESCHWINDIGKEIT 55
#define PZB_WACHSAM 58
#define PZB_FREI 59
#define PZB_BEFEHL 60
#define SIFA 61
#define HAUPTSCHALTER 62
#define MOTOR_SCHALTER 63
#define SCHALTER_FAHRTRICHTUNG 64
#define PFEIFE 65
#define SANDEN 66
#define TUEREN 67

//============================================================


extern Preferences preferences;       //api to store data (variable representing choice of interface) in nvs

typedef struct {
  MCP23017 mcp;
  bool enabled = false;
  uint8_t i2c; 
  char *address[16];
  char *info[16];
  uint16_t last_reading;
  uint16_t portMode = 0b0;
} MCP_Struct;

typedef struct {
  Adafruit_PCF8591 pcf;
  bool enabled = false;
  uint8_t i2c; 
  char *address[5];
  char *info[5];
  uint8_t last_reading[4];  //array to temporarily save analog reads of the four inputs of a pcf-Ic
} PCF_Struct;


extern MCP_Struct mcp_list[]; 
extern PCF_Struct pcf_list[];

extern uint8_t VERBOSE;           //0: no output, 1: infos, 
extern bool run_config_task;      //depending on value run serial_config_menu() (again) or suspend config_task (Task6)
/*
* storing i2c address[0] and pin number[1]  
* used to detect, which direction the throttle is beeing moved to
*/
extern uint8_t acceleration_button[]; 
extern uint8_t deceleration_button[];
extern bool acceleration_button_status;
extern bool deceleration_button_status;
extern uint8_t combined_throttle_ic[];

extern TaskHandle_t Task1;   //digital in
extern TaskHandle_t Task2;   //analog in
extern TaskHandle_t Task3;   //output
extern TaskHandle_t Task4;   //tx
extern TaskHandle_t Task5;   //rx
extern TaskHandle_t Task6;   //config 

extern QueueHandle_t serial_tx_verbose_queue;

extern SemaphoreHandle_t i2c_mutex;

//============================================================
//============================================================

#if ARDUINO_USB_CDC_ON_BOOT
#define HWSerial Serial0
#define USBSerial Serial
#else
#define HWSerial Serial
#endif

//============================================================
//W5500 ethernet shield (SPI)
//============================================================ 

#define DEBUG_ETHERNET_WEBSERVER_PORT       USBSerial
#define _ETHERNET_WEBSERVER_LOGLEVEL_       3

//not using default SPI-pins for int and cs, rest is default on ESP32-S3
//gpio numbers, not pin numbers on waveshare/arduino nano ESP32-S3 !!!
#define INT_GPIO            17 
#define CS_GPIO             18
#define MISO_GPIO           47
#define MOSI_GPIO           38
#define SCK_GPIO            48

//include lib after defining SPI-pins
#include <WebServer_ESP32_SC_W5500.h>

//load from NVS
extern byte mac[];      
extern IPAddress host;
extern uint16_t port;     //default is 1436                    

//transmitted every time, to request new data
extern byte handshake[]; //data to be sent to server after connection is established
extern size_t handshake_len;

extern byte request[];    //data to be sent to server after handshake 
#define REQUEST_LEN 27  //length of request array, instead of sizeof(request) because this number is part of the request_array itself

extern byte request_end[];
extern size_t request_end_len;

//count: number of outputs, of which some information is inside "message" 
typedef struct {
  char *message;
  int count;
} tcp_payload;

#endif