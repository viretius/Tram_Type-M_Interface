#include "config.h"

Preferences preferences;

bool config_menu;

MCP_Struct mcp_list[MAX_IC_COUNT]; 
PCF_Struct pcf_list[MAX_IC_COUNT];

uint8_t VERBOSE;
bool run_config_task;

uint8_t acceleration_button[2];
uint8_t deceleration_button[2];
uint8_t combined_throttle_pin[2];

TaskHandle_t Task1;   //digital input
TaskHandle_t Task2;   //analog input
TaskHandle_t Task3;   //output
TaskHandle_t Task4;   //serial rx / tcp rx
TaskHandle_t Task5;   //serial tx / keyboard tx
TaskHandle_t Task6;   //config menu

QueueHandle_t serial_tx_info_queue;
QueueHandle_t serial_tx_verbose_queue;

SemaphoreHandle_t i2c_mutex;

//  neopixelWrite(RGB_BUILTIN, RGB_BRIGHTNESS, 0, 0);  // Red
//  neopixelWrite(RGB_BUILTIN, 0, RGB_BRIGHTNESS, 0);  // Green
//  neopixelWrite(RGB_BUILTIN, 0, 0, RGB_BRIGHTNESS);  // Blue
