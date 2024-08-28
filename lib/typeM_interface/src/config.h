#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>

#include <Preferences.h> //for storing config in flash

#include <FS.h>
#include <LittleFS.h>
#include <FFat.h>
#include <CSV_Parser.h>

#include <USB.h>
#include <USBHIDKeyboard.h>

#include <Adafruit_PCF8591.h>                            
#include <MCP23017.h>       //slightly modded

//============================================================
//various global variables and definitions
//============================================================

#define MAX_IC_COUNT 8
#define interface_eeprom_address 0

#define ADC_STEP_THRESHOLD 5         //lower number results in higher sensitivity

#define DEBOUNCE_DELAY_MS 50

#define CMD_BUFFER_SIZE 10          // "X"(start) "U"(D/A) "XX"(address) "XXXX"(data) "Y"(end) "\n" 
#define INFO_BUFFER_SIZE 75         //buffer for infos about inputs, if run_config_task = true  
#define VERBOSE_BUFFER_SIZE 70 

#define MCP_I2C_BASE_ADDRESS 32     //0x20
#define PCF_I2C_BASE_ADDRESS 72     //0x48
#define MCP_I2C_END_ADDRESS (MCP_I2C_BASE_ADDRESS + MAX_IC_COUNT - 1)
#define PCF_I2C_END_ADDRESS (PCF_I2C_BASE_ADDRESS + MAX_IC_COUNT - 1)

#define CHECK_BIT(var, pos) ((var) & (1 << (pos))) 

extern Preferences preferences;       //api to store data (variable representing choice of interface) in nvs

extern bool config_menu;

typedef struct {
  MCP23017 mcp;
  bool enabled = false;
  uint8_t i2c; 
  char *address[16];
  uint16_t last_reading;
  uint16_t portMode = 0b0;
} MCP_Struct;

typedef struct {
  Adafruit_PCF8591 pcf;
  bool enabled = false;
  uint8_t i2c; 
  char *address[5];
  uint8_t last_reading[4];  //array to temporarily save analog reads of the four inputs of a pcf-Ic
} PCF_Struct;


extern MCP_Struct mcp_list[MAX_IC_COUNT]; 
extern PCF_Struct pcf_list[MAX_IC_COUNT];

extern uint8_t VERBOSE;
extern bool run_config_task;

/*
* storing i2c address[0] and pin number[1]  
* used to detect, which direction the throttle is beeing moved to
*/
extern uint8_t acceleration_button[2]; 
extern uint8_t deceleration_button[2];
extern uint8_t combined_throttle_pin[2];

extern TaskHandle_t Task1;   
extern TaskHandle_t Task2;
extern TaskHandle_t Task3;
extern TaskHandle_t Task4;
extern TaskHandle_t Task5;
extern TaskHandle_t Task6;

extern QueueHandle_t serial_tx_info_queue;
extern QueueHandle_t serial_tx_verbose_queue;

extern SemaphoreHandle_t i2c_mutex;
//============================================================
//USB stuff
//============================================================

#if ARDUINO_USB_CDC_ON_BOOT
#define HWSerial Serial0
#define USBSerial Serial
#else
#define HWSerial Serial
extern USBCDC USBSerial;
#endif

//============================================================
//W5500 ethernet shield (SPI)
//============================================================ 

#define DEBUG_ETHERNET_WEBSERVER_PORT       USBSerial
#define _ETHERNET_WEBSERVER_LOGLEVEL_       3
//not using default SPI-pins for int and cs
#define INT_GPIO            17 
#define CS_GPIO             18

//include lib after defining SPI-pins
#include <WebServer_ESP32_SC_W5500.h>

//IPAddress myIP(192, 168, 2, 232);
//IPAddress myGW(192, 168, 2, 1);
//IPAddress mySN(255, 255, 255, 0);
//Google DNS Server IP
//IPAddress myDNS(8, 8, 8, 8);


#endif
