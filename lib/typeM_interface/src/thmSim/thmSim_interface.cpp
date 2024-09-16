#include <thmSim/thmSim_interface.h>

using namespace thmSim_config;

namespace thmSim_interface {

//=================================================================================
//print every input status to the serial port, initialization-procedure for thmSim
//=================================================================================

void ini() 
{
  xSemaphoreTake(i2c_mutex, portMAX_DELAY);

  int t = 0, i = 0;
  uint16_t port = 0b0;    //GPIO register  
  uint8_t reading = 0;    //analog value
  uint8_t button_state = false;
  char data[5];;                         //4 chars + nullterminator
  char address[3];

  if (VERBOSE) {Serial.println("MCP ICs:");}
  for (i = 0; i < MAX_IC_COUNT; i++) 
  { 
    if(!mcp_list[i].enabled) {continue;} //IC with this i2c address is "deactivated"  
    port = mcp_list[i].mcp.read();  //read GPIO-Register
    if (VERBOSE) {printBinary(port);}
    
    for (t = 0; t < 16; t++) 
    { 
      if (strncmp(mcp_list[i].address[t], "-1", 2) == 0) {continue;} //pin not used
      if (!CHECK_BIT(mcp_list[i].portMode, t)) {continue;}   //pin set as output

      button_state = CHECK_BIT(port, t);                //
      bitWrite(mcp_list[i].last_reading, t, button_state); //update last_reading 
      
      if( (i == acceleration_button[0] - MCP_I2C_BASE_ADDRESS) && (t == acceleration_button[1]) ) 
      {
        acceleration_button_status = button_state; 
        if (VERBOSE) queue_printf<VERBOSE_BUFFER_SIZE>(serial_tx_verbose_queue, "\n[INI]\n  Beschleunigen\n  Status: %s\n", button_state? "1":"0");
        continue; 
      }
      if( (i == deceleration_button[0] - MCP_I2C_BASE_ADDRESS) && (t == deceleration_button[1]) )
      {
        deceleration_button_status = !(button_state);
        if (VERBOSE) queue_printf<VERBOSE_BUFFER_SIZE>(serial_tx_verbose_queue, "\n[INI]\n  Bremsen\n  Status: %s\n", button_state? "1":"0");
        continue;  
      }
           
      memset(&data[0], '\0', 5);
      memset(&address[0], '\0', 3);
      button_state ? strcpy(data, "0000") : strcpy(data, "0001");   //invert, because input-pullups pull inputs high, button-press pulls them low
      strcpy(address, mcp_list[i].address[t]);        
      
      Serial.printf("XU%02s%04sY", address, data); 
      Serial.flush(); 
    }
  }

  if (VERBOSE) {Serial.println("PCF ICs:");}

  for (i = 0; i < MAX_IC_COUNT; i++) 
  {       
    if(!pcf_list[i].enabled) {continue;} //IC with this i2c address is "deactivated"

    for (t = 0; t < 4; t++)
    {  
      if (pcf_list[i].address[t] == nullptr || strncmp(pcf_list[i].address[t], "-1", 2) == 0) {
          continue;
      }   
      memset(&data[0], '\0', 5);
      memset(&address[0], '\0', 3);   

      reading = pcf_list[i].pcf.analogRead(t);

      pcf_list[i].last_reading[t] = reading;

      sprintf(data, "%04X", reading);    //convert dec to hex

      if ((i + PCF_I2C_BASE_ADDRESS) == combined_throttle_ic[0] && t == combined_throttle_ic[1])
      { 
        if (acceleration_button_status) 
        {
          Serial.printf("XV%02s%04sY", mcp_list[acceleration_button[0] - MCP_I2C_BASE_ADDRESS].address[acceleration_button[1]], data); 
          if (VERBOSE) queue_printf<VERBOSE_BUFFER_SIZE>(serial_tx_verbose_queue, "\n[INI]\n  Beschleunigen\n  Kanal: %s\n", address);
          Serial.printf("XV%02s%04sY", mcp_list[deceleration_button[0] - MCP_I2C_BASE_ADDRESS].address[deceleration_button[1]], "0"); 
          continue;
        }
        else if (deceleration_button_status) 
        {
          Serial.printf("XV%02s%04sY", mcp_list[deceleration_button[0] - MCP_I2C_BASE_ADDRESS].address[deceleration_button[1]], data); 
          if (VERBOSE) queue_printf<VERBOSE_BUFFER_SIZE>(serial_tx_verbose_queue, "\n[INI]\n  Bremsen\n  Kanal: %s\n", address);
          Serial.printf("XV%02s%04sY", mcp_list[acceleration_button[0] - MCP_I2C_BASE_ADDRESS].address[acceleration_button[1]], "0"); 
          continue;
        }
        else { 
          Serial.printf("XV%02s%04sY", mcp_list[acceleration_button[0] - MCP_I2C_BASE_ADDRESS].address[acceleration_button[1]], "0"); 
          Serial.flush(); 
          Serial.printf("XV%02s%04sY", mcp_list[deceleration_button[0] - MCP_I2C_BASE_ADDRESS].address[deceleration_button[1]], "0"); 
          continue;
        }
      }
      else //some analog input 
      {
        strncat(address, pcf_list[i].address[t], 2); //just any other analog input
        if(VERBOSE) {Serial.printf("\nWert an Pin %i von PCF-IC mit Adresse %i: %u\n", t, i+72, data);}
      }
          
      Serial.printf("XV%02s%04sY", pcf_list[i].address[t], data);
      Serial.flush(); 
    }

  }

  Serial.print("INI2");
  xSemaphoreGive(i2c_mutex);
}

//=========================================================================================
//polls every connected MCP IC and checks for changes of the input pin states. 
//if a pin state changed, a command is created and added to the queue "serial_tx_cmd_queue"
//=========================================================================================

void digital_input_task (void * pvParameters)
{ 
  char cmd_buffer[CMD_BUFFER_SIZE] = {'\0'};              //stores resulting command depending on input that gets queued and later transmitted   
  char verbose_buffer[VERBOSE_BUFFER_SIZE] = {'\0'};
  char data[5];                         //4 chars + nullterminator
  
  uint16_t readingAB = 0b0;
  uint16_t ab_flag = 0b0;     //set bit indicates, which pin of a port changed 

  uint8_t t, i, j;                      //local for-loop counter

  TickType_t xLastWakeTime = xTaskGetTickCount(); //used for debounce delay

  for(;;)
  {
    vTaskDelay(5);
      
    for (i = 0; i < MAX_IC_COUNT; i++) 
    {
      if(!mcp_list[i].enabled) continue; //i2c adress not in use 

      if (xSemaphoreTake(i2c_mutex, pdMS_TO_TICKS(15)) == pdTRUE) {  //try to take the i2c_mutex, if not possible, continue with next IC
        readingAB = mcp_list[i].mcp.read();                           //read GPIO-Register
        xSemaphoreGive(i2c_mutex);
      }else {continue;}
      
      ab_flag = (readingAB ^ mcp_list[i].last_reading) & mcp_list[i].portMode; //check for state-changes of an input-pin
        
      if (!ab_flag) continue; //check if some input-pin state changed. If not, continue and move on to next IC
         
      xTaskDelayUntil( &xLastWakeTime, pdMS_TO_TICKS(DEBOUNCE_DELAY_MS) );  //suspend this task for the debounce-delay
        
      if (xSemaphoreTake(i2c_mutex, pdMS_TO_TICKS(15)) == pdTRUE) { //read GPIO-Register again, if not possible, continue with next IC
        readingAB = mcp_list[i].mcp.read(); 
        xSemaphoreGive(i2c_mutex);
      } else {continue;}

      ab_flag = (readingAB ^ mcp_list[i].last_reading) & mcp_list[i].portMode;   //check again for state-changes of an input-pin 

      if (!ab_flag) continue;                                   //check if pin-state is still the same after debounce delay...
             
      mcp_list[i].last_reading = readingAB;                      //...update last_reading

      if(VERBOSE) 
      {
        char *port_reading = new char[17]{'\0'};
        memset(port_reading, '\0', 17);
        for (int j = 0; j < 16; j++) { strcat(port_reading, (CHECK_BIT(readingAB, j) ? "1" : "0")); }
        queue_printf<VERBOSE_BUFFER_SIZE>(serial_tx_verbose_queue, "\n[digital_input_task]\n  GPIO Register: %s\n", port_reading);
      }


      //loop through every bit of ab_flag (every pin of an IC) and check, which bit is set
      for (t = 0; t <= 15; t++)            
      {    
        if (!CHECK_BIT(ab_flag, t)) continue; //if bit is not set, continue to check next pin                                      
        
        /*
        * these pins are used in analog_input_task, to indicate direction of the throttle / potentiometer
        * -> not relevant for this task, only update the status of the buttons, don't send any commands
        * 
        * first check if current i2c address matches to configured one in "acceleration_button" or "deceleration_button" at [0]
        * then check, if ab_flag has a set-bit at the index of the configured pin at [1]
        */
        if( (i == (acceleration_button[0] - MCP_I2C_BASE_ADDRESS)) && (t == acceleration_button[1]) ) 
        {
          acceleration_button_status = !(CHECK_BIT(readingAB, acceleration_button[1]) == 0); //ac-button is active high
          if (VERBOSE) 
          {
            snprintf(verbose_buffer, VERBOSE_BUFFER_SIZE,
              "  ac-Pin: %c%d\n  I2C-Adresse (DEC): %d\n  Tasterstatus: %u\n",
              (t > 7) ? 'B' : 'A', (t > 7) ? t - 8 : t,
              acceleration_button[0],
              acceleration_button_status
            );
            xQueueSend(serial_tx_verbose_queue, &verbose_buffer, pdMS_TO_TICKS(15));
          }
          continue; 
        }
        if( (i == (deceleration_button[0] - MCP_I2C_BASE_ADDRESS)) && (t == deceleration_button[1]) )
        {
          deceleration_button_status = !(CHECK_BIT(readingAB, deceleration_button[1]) == 0); //dec-button is active low
          if (VERBOSE) 
          {
            snprintf(verbose_buffer, VERBOSE_BUFFER_SIZE,
              "  dc-Pin: %c%d\n  I2C-Adresse (DEC): %d\n  Tasterstatus: %u\n",
              (t > 7) ? 'B' : 'A', (t > 7) ? t - 8 : t,
              deceleration_button[0],
              deceleration_button_status
            );
            xQueueSend(serial_tx_verbose_queue, &verbose_buffer, pdMS_TO_TICKS(15));
          }
          continue;     
        }
        //check wether the pin changed from 0 to 1 or 1 to 0 and store according char to "data" variable
        //-> thmSim expects inverted values
        (ab_flag & readingAB) ? strcpy(data, "0000") : strcpy(data, "0001") ;   

        //create command-string   
        snprintf(cmd_buffer, CMD_BUFFER_SIZE, "XU%02s%04sY", mcp_list[i].address[t], data);
        //put command-string into the queue                    
        if (!VERBOSE) xQueueSend(serial_tx_cmd_queue, &cmd_buffer, pdMS_TO_TICKS(1));   
        
        else {
          snprintf(verbose_buffer, VERBOSE_BUFFER_SIZE,
            "  Pin: %c%d\n  I2C-Adresse (DEC): %d\n  Datentelegram: %s\n",
            (t > 7) ? 'B' : 'A', (t > 7) ? t - 8 : t,
            i + MCP_I2C_BASE_ADDRESS,
            cmd_buffer
          );
          xQueueSend(serial_tx_verbose_queue, &verbose_buffer, pdMS_TO_TICKS(2));
        }              
      }

    }
  } 
}

//================================================================================================
//similar to digital_input_task. 
//difference is that the connected PCF ICs are responsible for AD conversion and constantly polled
//================================================================================================

void analog_input_task (void * pvParameters)
{
  char cmd_buffer[CMD_BUFFER_SIZE] = {'\0'}; 
  char verbose_buffer[VERBOSE_BUFFER_SIZE] = {'\0'}; 

  char data[5];                //4 chars + nullterminator, stores the converted hex-value of the analog input
  char address[3]; 

  uint8_t reading[4];           //every pcf-IC has 4 analog inputs
  uint8_t t, i, j;              //local for-loop counter

  for(;;)
  {
    vTaskDelay(5);
    
    for (i = 0; i < MAX_IC_COUNT; i++) 
    {
      if(!pcf_list[i].enabled) continue;//i2c adress not in use   

      for (int t = 0; t < 4; t++)   // read every analog Input (pins 0-3) of the PCF IC
      { 
        if (xSemaphoreTake(i2c_mutex, pdMS_TO_TICKS(15)) == pdTRUE) //try to take the i2c_mutex, 
        { 
          reading[t] = pcf_list[i].pcf.analogRead(t);
          xSemaphoreGive(i2c_mutex);
        } else {continue;}     //if not possible, continue with next IC                                       
        
        //check if the analog input changed significantly
        if (reading[t] > pcf_list[i].last_reading[t] + ADC_STEP_THRESHOLD || reading[t] < pcf_list[i].last_reading[t] - ADC_STEP_THRESHOLD)
        {          
          if (reading[t] <= ADC_STEP_THRESHOLD) reading[t] = 0; //value is below threshold, set it to 0
          else if (reading[t] >= 255 - ADC_STEP_THRESHOLD) reading[t] = 255;//value is above 255-threshold, set it to 255

          pcf_list[i].last_reading[t] = reading[t]; //update last_reading

          sprintf(data, "%04X", reading[t]);    //convert dec to hex

          //combined throttle has been moved
          if ((i + PCF_I2C_BASE_ADDRESS) == combined_throttle_ic[0] && t == combined_throttle_ic[1])
          { 
            if (acceleration_button_status) 
            {
              sprintf( address, "%02s", mcp_list[acceleration_button[0] - MCP_I2C_BASE_ADDRESS].address[acceleration_button[1]] );
              if (VERBOSE) queue_printf<VERBOSE_BUFFER_SIZE>(serial_tx_verbose_queue, "\n[analog input]\n  Beschleunigen\n  Kanal: %s\n", address);

            }
            else if (deceleration_button_status) 
            {
              sprintf( address, "%02s", mcp_list[deceleration_button[0] - MCP_I2C_BASE_ADDRESS].address[deceleration_button[1]] );
              if (VERBOSE) queue_printf<VERBOSE_BUFFER_SIZE>(serial_tx_verbose_queue, "\n[analog input]\n  Bremsen\n  Kanal: %s\n", address);
            }
            else {
              reading[t] = 0; //no button is pressed, set value to 0 to be on the safe side
              //dont overwrite address
              if (VERBOSE) queue_printf<VERBOSE_BUFFER_SIZE>(serial_tx_verbose_queue, "\n[analog input]\n  Mittelstellung.\n");

            }
            if (VERBOSE) queue_printf<VERBOSE_BUFFER_SIZE>(serial_tx_verbose_queue, "  Wert an Kombi-Hebel: %u\n", reading[t]);            
          }
            
          else //value of some analog input has changed
          {
            snprintf(address, 2, pcf_list[i].address[t]); //just any other analog input
            if(VERBOSE) queue_printf<VERBOSE_BUFFER_SIZE>(serial_tx_verbose_queue, "\n[analog input]\n  Wert an Pin %i von PCF-IC mit Adresse %i: %u\n", t, i+72, reading[t]);
          }
          
          //create command-string
          snprintf(cmd_buffer, CMD_BUFFER_SIZE, "XV%02s%04sY", address, data);

          //put command-string into the queue    
          if (!VERBOSE) xQueueSend(serial_tx_cmd_queue, &cmd_buffer, pdMS_TO_TICKS(5)); 
          else 
          {
            snprintf(verbose_buffer, VERBOSE_BUFFER_SIZE,
              "  Pin: A%d\n  I2C-Adresse (DEC): %d\n  Datentelegram: %s\n",
              t,
              i + PCF_I2C_BASE_ADDRESS,
              cmd_buffer
            );
            xQueueSend(serial_tx_verbose_queue, &verbose_buffer, pdMS_TO_TICKS(80));
          } 
                                              
        }
      }
    }

  } 
}

//==============================================================
//set outputs according to received data over Serial connection
//==============================================================

void output_task (void * pvParameters)
{ 
  
  char cmd_buffer[CMD_BUFFER_SIZE] = {'\0'}; 
  char verbose_buffer[VERBOSE_BUFFER_SIZE] = {'\0'};
  char address[3]; 
  char data[5]; 
  
  uint8_t t, i;  //local for-loop counter
  uint8_t value; //stores converted hex-value for analog outputs

  for(;;)
  {
    vTaskDelay(5);

    memset(&cmd_buffer[0], '\0', CMD_BUFFER_SIZE);          //clear cmd_buffer   
    xQueueReceive(serial_rx_cmd_queue, &cmd_buffer, portMAX_DELAY); //block this task indefinitely, if queue is empty
    
    //pointer to the first char after the command-char and the address-char (=second char of the command-string)
    char *buffer_ptr = cmd_buffer + 2; 

    if (cmd_buffer[1] == 'U') //digital output
    {
      if(VERBOSE) queue_printf<VERBOSE_BUFFER_SIZE>(serial_tx_verbose_queue, "\n[output task]\n  Datentelegramm empfangen: %s", cmd_buffer);
      
      strncpy(address, buffer_ptr, 2);   //copy first two chars to address
      buffer_ptr += 2;                   //remove those first two chars
      strncpy(data, buffer_ptr, 4);      //store remaining four chars in the data char-array, drop the last char ('Y')

      if(VERBOSE) queue_printf<VERBOSE_BUFFER_SIZE>(serial_tx_verbose_queue, "\n  Kanal: %s  Wert: %s", address, data);

      for (i = 0; i < MAX_IC_COUNT; i++) 
      {
        if(!mcp_list[i].enabled) continue; //i2c adress not in use
        
        for (t = 0; t < 16; t++) 
        {
          if(strncmp(mcp_list[i].address[t], address, 2) != 0) continue; //continue until finding the address
          
          if (CHECK_BIT(mcp_list[i].portMode, t)) //pin was set as input!
          { 
            if(VERBOSE) queue_printf<VERBOSE_BUFFER_SIZE>(serial_tx_verbose_queue, "\n  Pin %i ist als Eingang konfiguriert.\n", t);
            continue; //maybe there is a output with the same address?
          }

          if (xSemaphoreTake(i2c_mutex, pdMS_TO_TICKS(20)) == pdTRUE)   //pin with this address was found, try to take the i2c_mutex
          { 
            if(VERBOSE) queue_printf<VERBOSE_BUFFER_SIZE>(serial_tx_verbose_queue, "\n  PIN %i an IC mit i2c-Adresse %i wird geschaltet\n", t, i+MCP_I2C_BASE_ADDRESS);
            mcp_list[i].mcp.digitalWrite(t, atoi(data)); //set output
            xSemaphoreGive(i2c_mutex); //release mutex
          } 
          else { //mutex could not be taken after 20ms
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

      if(VERBOSE) queue_printf<VERBOSE_BUFFER_SIZE>(serial_tx_verbose_queue, "\n  Kanal: %s  Wert: %s ", address, data);

      for (i = 0; i < MAX_IC_COUNT; i++) 
      {
        if(!pcf_list[i].enabled) continue; //i2c adress not in use
        if(strcmp(pcf_list[i].address[t], address) != 0) continue; //continue until finding the address
        value = strtol(data, NULL, 16);  // Hex-Wert in Dezimalwert umwandeln
        if(VERBOSE) queue_printf<VERBOSE_BUFFER_SIZE>(serial_tx_verbose_queue, "\n  Setze analog ausgang mit Wert: %u\n", value);

        value = map(value, 0, 255, 0, 30);
        if (xSemaphoreTake(i2c_mutex, pdMS_TO_TICKS(20)) == pdTRUE) { 
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

//=========================================
//receive data and add to according queues
//=========================================

void rx_task(void *pvParameters)
{
    char cmd_buffer[CMD_BUFFER_SIZE] = {'\0'};
    char verbose_buffer[VERBOSE_BUFFER_SIZE] = {'\0'};
    int bytesRead;


    for(;;)
    {
      vTaskDelay(2);

      if (Serial.available() > 0) // hw rx buffer holds at least one char
      {
        bytesRead = 0;
        memset(&cmd_buffer[0], '\0', CMD_BUFFER_SIZE);
        cmd_buffer[0] = Serial.peek();

        if (cmd_buffer[0] == 'M' || cmd_buffer[0] == 'm') vTaskResume(config_task_handle);
      
        else if (cmd_buffer[0] == 'I')
        {
          //sometimes it happens, that the esp reads the serial buffer faster, than there are bytes transmitted
          //(especially with a slow rate of 9600baud)
          //-> read until hardware-buffer ist empty
          //if desired number of bytes couldnt be read, the while-loop will run again
          while (Serial.available()) 
          {                                                     //INI1 -> 4 bytes
            bytesRead += Serial.readBytes(cmd_buffer + bytesRead, 4 - bytesRead);  
            if (bytesRead > 3 || cmd_buffer[0] == '\0') break; 
            vTaskDelay(1); 
          }
          if (strncmp(cmd_buffer, "INI1", 4) == 0) ini();
          else if (VERBOSE) //command is not complete
          {
            queue_printf<VERBOSE_BUFFER_SIZE>(serial_tx_verbose_queue, "\n[Serial_rx_task]\n  Fehlerhaftes Datentelegramm: %s\n", cmd_buffer);
          }
        }
        else if (cmd_buffer[0] == 'X')
        {
          while (Serial.available()) 
          {                                                 //XUxxxxxxY -> 9 Bytes
            bytesRead += Serial.readBytes(cmd_buffer + bytesRead, 9 - bytesRead);  
            if (bytesRead > 8 || cmd_buffer[0] == '\0') break;
            vTaskDelay(1); 
          }
          if (cmd_buffer[8] == 'Y')
          {
            xQueueSend(serial_rx_cmd_queue, &cmd_buffer, pdMS_TO_TICKS(2));
          }
          else if (VERBOSE) //command in the wrong format
          {
            queue_printf<VERBOSE_BUFFER_SIZE>(serial_tx_verbose_queue, "\n[Serial_rx_task]\n  Fehlerhaftes Datentelegramm: %s\n", cmd_buffer);
          }
        }
        else 
        {
          // Read the available bytes if any unknown command
          size_t availableBytes = Serial.available();
          if (availableBytes > 0)
          {
            Serial.readBytes(cmd_buffer, availableBytes);
            if (VERBOSE) queue_printf<VERBOSE_BUFFER_SIZE>(serial_tx_verbose_queue, "\n[Serial_rx_task]\n  Fehlerhaftes Datentelegramm: %s\n", cmd_buffer);           
          }
        }
        while(Serial.available()) Serial.read(); //clear hardware buffer
      }
    }
}


//=============================
//transmit data from queues
//=============================

 void tx_task (void * pvParameters)
{ 
  char buffer[VERBOSE_BUFFER_SIZE] = {'\0'}; 

  for(;;)
  {
    vTaskDelay(2);
    memset(&buffer[0], '\0', VERBOSE_BUFFER_SIZE);              //clear cmd char array                       

    if ((!VERBOSE) && xQueueReceive(serial_tx_cmd_queue, &buffer, 0) == pdTRUE)  Serial.print(buffer); 
    
    if (VERBOSE && xQueueReceive(serial_tx_verbose_queue, &buffer, 0) == pdTRUE) Serial.print(buffer);
    
  }
}

//===============================================================================
//configuration menu
//own task to be able to suspend and resume other tasks from within this function
//===============================================================================
void config_task (void * pvParameters)
{

  for(;;)
  {
    vTaskDelay(2);

    //suspend tasks to prevent data corruption, if user changes configuration
    //also there is simply no need to poll inputs or outputs, while in the config menu
    vTaskSuspend(digital_input_task_handle);
    vTaskSuspend(analog_input_task_handle);
    vTaskSuspend(output_task_handle);
    vTaskSuspend(rx_task_handle);
    vTaskSuspend(tx_task_handle);

    serial_config_menu();  

    if (!run_config_task) //
    {
      vTaskResume(digital_input_task_handle);
      vTaskResume(analog_input_task_handle);
      vTaskResume(output_task_handle);
      vTaskResume(rx_task_handle);
      vTaskResume(tx_task_handle);
      vTaskSuspend(NULL);
    }
  }
}
//=============================================================
//=============================================================
void init_interface() 
{

    Wire.begin();       //initialize i2c bus
    if (!load_config()) return; //load config from files
    
    //queues for communication between tasks: 
    serial_rx_cmd_queue = xQueueCreate(4, CMD_BUFFER_SIZE*sizeof(char));            //receive c
    serial_tx_cmd_queue = xQueueCreate(4, CMD_BUFFER_SIZE*sizeof(char));          //stores commands, that are created in both input-tasks and then transmitted by the tx_task 
    serial_tx_verbose_queue = xQueueCreate(4, VERBOSE_BUFFER_SIZE*sizeof(char));   
    
    i2c_mutex = xSemaphoreCreateMutex();

    //create tasks
    xTaskCreate(digital_input_task, "Task1", 2000, NULL, 2, &digital_input_task_handle);
    xTaskCreate(analog_input_task, "Task2", 2000, NULL, 2, &analog_input_task_handle);
    xTaskCreate(output_task, "Task3", 3000, NULL, 2, &output_task_handle);
    xTaskCreate(
      rx_task,              /* Task function. */
      "Task4",              /* name of task. */
      10000,                 /* Stack size of task */
      NULL,                 /* parameter of the task */
      1,                    /* priority of the task */
      &rx_task_handle               /* Task handle to keep track of created task */                 
    );
    xTaskCreate(tx_task, "Task5", 2000, NULL, 2, &tx_task_handle);
    
    xTaskCreate(config_task, "Task6", 8000, NULL, 2, &config_task_handle);
    vTaskSuspend(config_task_handle); //dont open the config menu
    
    Serial.printf(("\nTasks erfolgreich gestartet.\nUm in das Konfigurationsmenü zu gelangen, \"M\" eingeben."));
    
} //init

} //namespace trainSim_interface
