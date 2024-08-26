#include "config.h"

Preferences preferences;

bool config_menu;

MCP_Struct mcp_list[MAX_IC_COUNT]; 
PCF_Struct pcf_list[MAX_IC_COUNT];

uint8_t VERBOSE;
bool run_config_task;

uint8_t acceleration_button[2] = {34, 2};
uint8_t deceleration_button[2] = {33, 8};
uint8_t combined_throttle_pin[2] = {73, 0};

TaskHandle_t Task1;   
TaskHandle_t Task2;
TaskHandle_t Task3;
TaskHandle_t Task4;
TaskHandle_t Task5;
TaskHandle_t Task6;

QueueHandle_t serial_tx_info_queue;
QueueHandle_t serial_tx_verbose_queue;

SemaphoreHandle_t i2c_mutex;