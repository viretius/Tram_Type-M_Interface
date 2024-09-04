

//================================================================================================================
//================================================================================================================


#include <simMetro_interface/simMetro_interface.h>

using namespace simMetro_config;

namespace simMetro_interface {

//================================================================================================================

//========================================================================================================

/*inline float int_to_volts ( uint16_t dac_value, uint8_t bits ) 
{  
  return (((float)dac_value / ((1 << bits) - 1)) * ADC_REFERENCE_VOLTAGE);      //dac value (uint8_t) -> cast to float and divide by 127 (8 bit resolution), multiply by referene voltage. e.g. 127/(128-1) * 5.0V = 5.0V
};*/



//========================================================================================================

/*
*pwm output for buzzer
*/

 void toggle_buzzer(uint8_t pin, float frequency) 
{

}

//========================================================================================================
//========================================================================================================


//================================================================================================================
//polls every connected MCP IC and checks for changes of the input pin states. Interrupt not used, because only the i2c bus is connected for compatibility reasons
//if a pin state changed, a command is created and added to the queue "serial_tx_cmd_queue"
//================================================================================================================

 void digital_input_task (void * pvParameters)
{ 
  uint16_t readingAB = 0b0;
  uint16_t ab_flag = 0b0;     //set bit indicates, which pin of a port changed 
  
  char cmd_buffer[CMD_BUFFER_SIZE] = {'\0'};              //stores resulting command depending on input that gets queued and later transmitted   
  char verbose_buffer[VERBOSE_BUFFER_SIZE] = {'\0'};
  char data[5];                         //4 chars + nullterminator
  uint8_t t, i, j;                      //lokal for-loop counter

  TickType_t xLastWakeTime = xTaskGetTickCount(); //used for debounce delay

  for(;;)
  {
    vTaskDelay(5);
      
    for (i = 0; i < MAX_IC_COUNT; i++) 
    {
      if(!mcp_list[i].enabled) continue; //IC with this i2c address is "deactivated"

      if (xSemaphoreTake(i2c_mutex, pdMS_TO_TICKS(30)) == pdTRUE) { 
        readingAB = mcp_list[i].mcp.read(); //port A: LSB
        xSemaphoreGive(i2c_mutex);
      }else {continue;}
      
      ab_flag = (readingAB ^ mcp_list[i].last_reading) & mcp_list[i].portMode; 
             
       /*
       * these pins are used in analog_input_task, to indicate direction of the throttle / potentiometer
       * -> not relevant for this task
       *
       * first check if current i2c address matches to configured one
       * then check, if ab_flag has a set-bit at the index of one of the configured buttons
       */
 
      if (!ab_flag) continue; //check if some input-pin state changed. If not, check continue and check next IC
         
      xTaskDelayUntil( &xLastWakeTime, pdMS_TO_TICKS(DEBOUNCE_DELAY_MS) );  //suspend this task and free cpu resources for the specific debounce delay
        
      if (xSemaphoreTake(i2c_mutex, pdMS_TO_TICKS(30)) == pdTRUE) {
        readingAB = mcp_list[i].mcp.read(); //port A: LSB
        xSemaphoreGive(i2c_mutex);
      } else {continue;}

      ab_flag = (readingAB ^ mcp_list[i].last_reading) & mcp_list[i].portMode;   //check again for state-changes of an input-pin 

      if (!ab_flag) continue;  //check if pin-state is still the same after debounce delay
             
      mcp_list[i].last_reading = readingAB;     //store last portreading

      if(VERBOSE) 
      {
        char *port = new char[17]{'\0'};
        memset(port, '\0', 17);
        for (int j = 0; j < 16; j++) { strcat(port, (CHECK_BIT(readingAB, j) ? "1" : "0")); }
        queue_printf<VERBOSE_BUFFER_SIZE>(serial_tx_verbose_queue, "\n[digital_input_task]\n  GPIO Register: %s\n", port);
      }

      //continue, if acceleration/deceleration button status changed (only relevant for analog in task)
      if(i == acceleration_button[0] - MCP_I2C_BASE_ADDRESS) 
      {
        acceleration_button_status = CHECK_BIT(readingAB, acceleration_button[1]) == 0;
        if (acceleration_button_status) continue; 
      }
      if (i == deceleration_button[0] - MCP_I2C_BASE_ADDRESS) 
      {
        deceleration_button_status = CHECK_BIT(readingAB, deceleration_button[1]) == 0;
        if (deceleration_button_status) continue;
      }

      //loop through every bit of ab_flag (every pin of every connected IC) and check, which bit is set
      //send according data
      for (t = 0; t < 16; t++)            
      {                                         
        if (!CHECK_BIT(ab_flag, t)) continue;      
        if(i == acceleration_button[0] - MCP_I2C_BASE_ADDRESS && t == acceleration_button[1]) continue; 
        if(i == deceleration_button[0] - MCP_I2C_BASE_ADDRESS && t == deceleration_button[1]) continue; 
                      
        (ab_flag & readingAB) ? strcpy(data, "0000") : strcpy(data, "0001") ;   //check wether the pin changed from 0 to 1 or 1 to 0 and store according char to "data" variable, inverted because of the implementation in the simulation programm
              
        snprintf(cmd_buffer, CMD_BUFFER_SIZE, "XU%02s%04sY", mcp_list[i].address[t], data);
                          
        if ((!VERBOSE)) xQueueSend(serial_tx_cmd_queue, &cmd_buffer, pdMS_TO_TICKS(1));  
        
        else {
          snprintf(verbose_buffer, VERBOSE_BUFFER_SIZE,
            "\n[digital_input_task]\n  Pin: %c%d\n  I2C-Adresse (DEC): %d\nDatentelegram: %s",
            (t > 7) ? 'B' : 'A', 
            (t > 7) ? t - 8 : t,
            i + MCP_I2C_BASE_ADDRESS,
            cmd_buffer
          );
          xQueueSend(serial_tx_verbose_queue, &verbose_buffer, pdMS_TO_TICKS(1));
        }              
      }

    }
  } 
}

//================================================================================================================
//similar to digital_input_task. difference is that the connected PCF ICs are responsible for AD conversion and constantly polled
//================================================================================================================

 void analog_input_task (void * pvParameters)
{
  
  char cmd_buffer[CMD_BUFFER_SIZE] = {'\0'}; 
  char verbose_buffer[VERBOSE_BUFFER_SIZE] = {'\0'}; 
  char data[5];

  uint8_t reading[4];           //every pcf-IC has 4 analog inputs
  uint8_t t, i, j;              //lokal for-loop counter

  TickType_t intervall = pdMS_TO_TICKS(1000); //needed for debug output

  for(;;)
  {
    vTaskDelay(5);
    
    for (i = 0; i < MAX_IC_COUNT; i++) 
    {
      if(!pcf_list[i].enabled) continue;//i2c adress not in use   

      for (int t = 0; t < 4; t++)   // read every analog Input
      { 
        if (xSemaphoreTake(i2c_mutex, pdMS_TO_TICKS(30)) == pdTRUE) { 
          reading[t] = pcf_list[i].pcf.analogRead(t);
          xSemaphoreGive(i2c_mutex);
        } else {continue;}                                            
        
        if (reading[t] > pcf_list[i].last_reading[t] + ADC_STEP_THRESHOLD || reading[t] < pcf_list[i].last_reading[t] - ADC_STEP_THRESHOLD)// || reading[t] <= ADC_STEP_THRESHOLD || reading[t] >= 255-ADC_STEP_THRESHOLD ) //filter out inaccuracy of mechanics and the ADC
        {
          if(VERBOSE) queue_printf<VERBOSE_BUFFER_SIZE>(serial_tx_verbose_queue, "\n[analog input]\n  Wert an Pin %i von PCF-IC mit Adresse %i: %u\n", t, i+72, reading[t]);
          
          if (reading[t] <= ADC_STEP_THRESHOLD) reading[t] = 0;
          else if (reading[t] >= 255 - ADC_STEP_THRESHOLD) reading[t] = 255;

          pcf_list[i].last_reading[t] = reading[t];

          sprintf(data, "%04X", reading[t]);    

          memset(&cmd_buffer[0], '\0', CMD_BUFFER_SIZE); //clear cmd char array

          char address[3] = {0,0, 0};
          memset(&address[0], '\0', 3); 

          //combined throttle
          if ((i + PCF_I2C_BASE_ADDRESS) == combined_throttle_ic[0] && t == combined_throttle_ic[1]) //&& reading[t] >= ADC_STEP_THRESHOLD && reading[t] <= 25 - ADC_STEP_THRESHOLD) ;
          { 
            if (VERBOSE) queue_printf<VERBOSE_BUFFER_SIZE>(serial_tx_verbose_queue, "\n[analog input]\n  Wert an Kombi-Hebel: %u\n", reading[t]);            

            if (acceleration_button_status) 
            {
              if (VERBOSE) USBSerial.println("beschleunigen");
              strcat( address, mcp_list[acceleration_button[0] - MCP_I2C_BASE_ADDRESS].address[acceleration_button[1]] );
            }
            else if (deceleration_button_status) 
            {
              if (VERBOSE) USBSerial.println("bremsen");
              strcat( address, mcp_list[deceleration_button[0] - MCP_I2C_BASE_ADDRESS].address[deceleration_button[1]] );
            }
            else if (VERBOSE) queue_printf<VERBOSE_BUFFER_SIZE>(serial_tx_verbose_queue, "\n[analog input]\n  Kein Button gedrückt.\n");
            else continue;
          }
          else strncat(address, pcf_list[i].address[t], 2); //just any other analog input
          
          //create command-string
          snprintf(cmd_buffer, CMD_BUFFER_SIZE, "XV%02s%04sY", address, data);
                
          if (!VERBOSE) xQueueSend(serial_tx_cmd_queue, &cmd_buffer, pdMS_TO_TICKS(1)); 
          else {
            snprintf(verbose_buffer, VERBOSE_BUFFER_SIZE,
              "\n[analog_input_task]\n  Pin: %d\n  I2C-Adresse (DEC): %d\nDatentelegram: %s",
              t,
              i + PCF_I2C_BASE_ADDRESS,
              cmd_buffer
            );
            xQueueSend(serial_tx_verbose_queue, &verbose_buffer, pdMS_TO_TICKS(20));
          }                                           
        }
      }
    }

  } 
}

//================================================================================================================
//set outputs according to received data over USBserial connection
//================================================================================================================

 void output_task (void * pvParameters)
{ 
  
  char cmd_buffer[CMD_BUFFER_SIZE] = {'\0'}; 
  char verbose_buffer[VERBOSE_BUFFER_SIZE] = {'\0'};
  char address[3]; //= {'\0'}; 
  char data[5]; //= {'\0'};
  
  uint8_t t, i;                    //lokal for-loop counter
  uint8_t value; //variable to hold from char to hex converted value for an analog output

  for(;;)
  {
    vTaskDelay(5);

    memset(&cmd_buffer[0], '\0', CMD_BUFFER_SIZE);              //clear cmd char array   
    xQueueReceive(serial_rx_cmd_queue, &cmd_buffer, portMAX_DELAY); //block this task, if queue is empty

    char *buffer_ptr = cmd_buffer + 2;        //store buffer in buffer_ptr, remove first two chars (indicator for cmd-start & indicator for digital/analog data)

    if (cmd_buffer[1] == 'U')                   //digital output
    {
      if(VERBOSE) queue_printf<VERBOSE_BUFFER_SIZE>(serial_tx_verbose_queue, "\n[output task]\n  Datentelegramm empfangen: %s", cmd_buffer);
      
      strncpy(address, buffer_ptr, 2);   //copy first two chars to address
      buffer_ptr += 2;                   //remove those first two chars
      strncpy(data, buffer_ptr, 4);      //store remaining four chars in the data char-array

      if(VERBOSE) queue_printf<VERBOSE_BUFFER_SIZE>(serial_tx_verbose_queue, "\n  Kanal: %s  Wert: %s", address, data);

      for (i = 0; i < MAX_IC_COUNT; i++) 
      {
        if(!mcp_list[i].enabled) continue;//i2c adress not in use
        for (t = 0; t < 16; t++) 
        {
          if(strncmp(mcp_list[i].address[t], address, 2) != 0) continue;
          
          if (CHECK_BIT(mcp_list[i].portMode, t)) //pin was set as input!
          { 
            if(VERBOSE) queue_printf<VERBOSE_BUFFER_SIZE>(serial_tx_verbose_queue, "\n  Pin %i ist als Eingang konfiguriert.\n", t);
            goto ret; //break both for-loops;
          }

          if (xSemaphoreTake(i2c_mutex, pdMS_TO_TICKS(15)) == pdTRUE) 
          { 
            if(VERBOSE) queue_printf<VERBOSE_BUFFER_SIZE>(serial_tx_verbose_queue, "\n  PIN %i an IC mit i2c-Adresse %i wird geschaltet\n", t, i+MCP_I2C_BASE_ADDRESS);
            mcp_list[i].mcp.digitalWrite(t, atoi(data)); 
            xSemaphoreGive(i2c_mutex);
          }
          else {
            if(VERBOSE) queue_printf<VERBOSE_BUFFER_SIZE>(serial_tx_verbose_queue, "\n  PIN %i an IC mit i2c-Adresse %i konnte nicht geschaltet werden.\n", t, i+MCP_I2C_BASE_ADDRESS);
          }
          goto ret;  //address was found, no need to look further -> break both for-loops 
        }
      }
      ret: ;
    
    }

    else if (cmd_buffer[1] =='V')           //analog output
    {
      t = 4; //pcf ICs only have one output. related channel is stored at pcf_list[4]
      if(VERBOSE) queue_printf<VERBOSE_BUFFER_SIZE>(serial_tx_verbose_queue, "\n[output task]\n  Datentelegramm empfangen: %s", cmd_buffer);
      
      strncpy(address, buffer_ptr, 2);   //copy first two chars to address+
      buffer_ptr += 2;                   //remove those first two chars
      strncpy(data, buffer_ptr, 4);      //store remaining four chars in the data char-array

      if(VERBOSE) queue_printf<VERBOSE_BUFFER_SIZE>(serial_tx_verbose_queue, "\n  Kanal: %s  Wert: %s \n", address, data);

      for (i = 0; i < MAX_IC_COUNT; i++) 
      {
        if(!pcf_list[i].enabled) continue;
        if(strcmp(pcf_list[i].address[t], address) != 0) continue;
        
        sscanf(data, "%04x", &value); //convert hex to dec
        
        if (xSemaphoreTake(i2c_mutex, portMAX_DELAY) == pdTRUE) { 
          pcf_list[i].pcf.analogWrite(value);
          xSemaphoreGive(i2c_mutex);
        } 
        break;    
      }
    }  

    else {
      if(VERBOSE) queue_printf<VERBOSE_BUFFER_SIZE>(serial_tx_verbose_queue, "\n[output task]\n  Fehlerhaftes Datentelegramm empfangen: %s\n", cmd_buffer);
    }

  } 
}

//================================================================================================================
//receive data and add to according queues
//================================================================================================================

void rx_task (void * pvParameters)
{
  char cmd_buffer[CMD_BUFFER_SIZE] = {'\0'};
  char verbose_buffer[VERBOSE_BUFFER_SIZE] = {'\0'};
  uint8_t t, i;                                                   //lokal for-loop counter
  
  for(;;)
  {
    vTaskDelay(5);

    if (USBSerial.available() > 0) //hw rx buffer holds at least one char
    {
      USBSerial.readBytesUntil('\n', cmd_buffer, CMD_BUFFER_SIZE);
                                     
      if (cmd_buffer[0] == 'M' || cmd_buffer[0] == 'm') vTaskResume(Task6);
      
      else if (cmd_buffer[0] == 'X' && cmd_buffer[8] == 'Y') xQueueSend(serial_rx_cmd_queue, &cmd_buffer, pdMS_TO_TICKS(2)); //portMAX_DELAY
      
      else {
        if(VERBOSE) queue_printf<VERBOSE_BUFFER_SIZE>(serial_tx_verbose_queue, "\n[Serial_rx_task]\n  Fehlerhaftes Datentelegramm: %s\n", cmd_buffer); 
      }
      
    }
  }
}

//================================================================================================================
//transmit data from queues
//================================================================================================================

 void tx_task (void * pvParameters)
{ 
  char buffer[VERBOSE_BUFFER_SIZE] = {'\0'}; 

  for(;;)
  {
    vTaskDelay(2);
    memset(&buffer[0], '\0', VERBOSE_BUFFER_SIZE);              //clear cmd char array                       

    if ((!VERBOSE) && xQueueReceive(serial_tx_cmd_queue, &buffer, 0) == pdTRUE)  USBSerial.print(buffer); 
    
    if (VERBOSE && xQueueReceive(serial_tx_verbose_queue, &buffer, 0) == pdTRUE) USBSerial.print(buffer);
    
  }
}

/*
*configuration menu
*own task to be able to suspend and resume other tasks from within this function
*dont judge me for the code quality, it's just a configuration menu and i am not a professional programmer
*/

void config_task (void * pvParameters)
{

  for(;;)
  {
    vTaskDelay(5);

    //suspend tasks to prevent data corruption
    vTaskSuspend(Task1);      //digital input task
    vTaskSuspend(Task2);      //analog input task
    vTaskSuspend(Task4);      //USBSerial rx task     
    vTaskSuspend(Task5);      //USBSerial tx task

    serial_config_menu();  

    if (!run_config_task) //
    {
      vTaskResume(Task1);
      vTaskResume(Task2);
      vTaskResume(Task4);
      vTaskResume(Task5);
      vTaskSuspend(NULL);
    }
  }
}

//================================================================================================================
//================================================================================================================
void init() 
{

    Wire.begin();
    while(!USBSerial);

    if (!load_config()) return;
    
    /*
    serial_rx_cmd_queue:
    -1 slot with size of an integer, FIFO principal, stores commands "executed" in output_task
    -only for com between tasks. only one command (of size (CMD_BUFFER_SIZE)) large, because the hardware rx buffer of the esp32 is 256 bytes large
    */
          
    
    serial_rx_cmd_queue = xQueueCreate(4, CMD_BUFFER_SIZE*sizeof(char));        //receive c
    serial_tx_cmd_queue = xQueueCreate(4, CMD_BUFFER_SIZE*sizeof(char));   //stores commands, that are created in both input-tasks and then transmitted by the tx task through the USBserial com port
    serial_tx_verbose_queue = xQueueCreate(4, VERBOSE_BUFFER_SIZE*sizeof(char));        //already declared in confif, because this queue is used in both sims
    
    i2c_mutex = xSemaphoreCreateMutex();

    xTaskCreate(digital_input_task, "Task1", 2000, NULL, 2, &Task1);
    xTaskCreate(analog_input_task, "Task2", 2000, NULL, 2, &Task2);
    xTaskCreate(output_task, "Task3", 2000, NULL, 2, &Task3);
    xTaskCreate(
      rx_task,              /* Task function. */
      "Task4",              /* name of task. */
      2000,                 /* Stack size of task */
      NULL,                 /* parameter of the task */
      1,                    /* priority of the task */
      &Task4               /* Task handle to keep track of created task */                   
    );
    xTaskCreate(tx_task, "Task5", 2000, NULL, 2, &Task5);
    xTaskCreate(config_task, "Task6", 8000, NULL, 2, &Task6);
    vTaskSuspend(Task6);

    //indicate_finished_setup();
    
    USBSerial.printf(("Tasks erfolgreich gestartet.\nUm in das Konfigurationsmenü zu gelangen, \"M\" eingeben."));
    
} //init

} //namespace trainSim_interface
