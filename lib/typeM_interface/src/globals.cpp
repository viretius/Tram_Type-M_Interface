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

TaskHandle_t digital_input_task_handle;  
TaskHandle_t analog_input_task_handle;   
TaskHandle_t output_task_handle;   
TaskHandle_t rx_task_handle;   
TaskHandle_t tx_task_handle;   
TaskHandle_t config_task_handle;   

//queue for verbose output - used in both interfaces, thus declared globally
QueueHandle_t serial_tx_verbose_queue;
//mutex to prevent concurrent access to i2c-bus
SemaphoreHandle_t i2c_mutex;



