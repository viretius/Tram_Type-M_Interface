#ifndef GLOBALS_H
#define GLOBALS_H

#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>

#include <FS.h>
#include <LittleFS.h>
#include <CSV_Parser.h>
#include <Preferences.h>  


#if ( ARDUINO_NANO_ESP32)
#include <USB.h>
#endif

#include <Adafruit_PCF8591.h>                            
#include <MCP23017.h>  //slightly modded to be able to detect connected ICs

//============================================================
//various global variables, definitions and macros
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

//========================================================================
//api to store variables (choice of interface and network settings) in nvs
extern Preferences preferences; 
//========================================================================

extern uint8_t VERBOSE;           //0: no verbose output, 1: verbose output
extern bool run_config_task;      //depending on value: run serial_config_menu() (again) or suspend config_task 

typedef struct {
  MCP23017 mcp;
  bool enabled = false;
  uint8_t i2c;            //i2c address
  char *address[16];      //array of strings, containing data from config-file
  char *info[16];         //info about the inputs/outputs of the IC
  uint16_t last_reading;  
  uint16_t portMode = 0b0;  //Port-Configuration of the IC
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

/*
* storing i2c-address at [0] and pin-number at [1]  
* used to detect, which direction the throttle is beeing moved to
*/
extern uint8_t acceleration_button[]; 
extern uint8_t deceleration_button[];
extern bool acceleration_button_status;
extern bool deceleration_button_status;
//storing i2c-address at [0] and pin-number at [1], 
//there is only one combined throttle, so no 2d array needed
extern uint8_t combined_throttle_ic[];

//global task handles used in both interfaces
extern TaskHandle_t digital_input_task_handle;  
extern TaskHandle_t analog_input_task_handle;   
extern TaskHandle_t output_task_handle;   
extern TaskHandle_t rx_task_handle;   
extern TaskHandle_t tx_task_handle;   
extern TaskHandle_t config_task_handle;  

extern QueueHandle_t serial_tx_verbose_queue;

extern SemaphoreHandle_t i2c_mutex;

//=============================
//function prototype
//=============================

template <size_t count> //template to allow dynamic buffer size
void queue_printf(QueueHandle_t queue, const char *format, ...) {
    
    static char buffer[count] = {'\0'}; 

    va_list args;
    va_start(args, format);
    vsnprintf(buffer, count, format, args);
    va_end(args);
    
    // Send buffer contents to the queue
    if (xQueueSend(queue, &buffer, pdMS_TO_TICKS(10)) != pdTRUE) {
        Serial.println("Queue full\n");
        Serial.print(buffer);
    }
}

#endif