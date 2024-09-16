#include <lokSim3D/lokSim3D_interface.h>
#if ( ARDUINO_NANO_ESP32)

using namespace lokSim3D_config;

//extern variables, declared in the header file, initialized here
byte mac[6];    
IPAddress host;
uint16_t port;     //default is 1435 for LOKSIM3D  

//=============================================================
namespace lokSim3D_interface {

  //============================================================
  //variable initializations
  //============================================================
    
  //data to be sent to server after connection is established 
  byte handshake[] = {
    0x17 /**/, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01/*Version*/, 0x02/*CLIENT_TYPE*/, 0x12,                    
    'F','a','h','r','p','u','l','t',0x20,'T','y','p','-','M',0x20,'(','E','S','P','3','2','-','S','3',')'
  };   
    
  size_t handshake_len = sizeof(handshake);

  byte request[] = {
    //this array has a specific size
    //if it is altered, NUM_STATS has to be adjusted
    NUM_STATS + 4, 
    /*some unknown bytes for the server: */ 
    0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x0A,
    //requested data-sets
    GESCHWINDIGKEIT, AFB_SOLL_GESCHWINDIGKEIT, LM_Hauptschalter, 
    LM_MG_BREMSE, LM_H_BREMSE, LM_R_BREMSE, 
    LM_TUEREN
  };

  size_t request_len = sizeof(request);
  
  byte request_end[] = { 0x04, 0x00, 0x00, 0x00, 0x00,  0x03, 0x00, 0x00 };
  size_t request_end_len = sizeof(request_end);


//===============================================================
//handshake with Zusi 2 server with additional request for data 
//===============================================================

void handshake_and_request() {

  size_t len;
  uint8_t *ack;

  ///////////////////////////////////////////
  //send handshake to server and wait for ack
  ///////////////////////////////////////////

  if (VERBOSE) xQueueSend(serial_tx_verbose_queue, "\nHandshake\n   Handshake wird durchgeführt\n", pdMS_TO_TICKS(1));

  client.write(handshake, handshake_len);

  if (VERBOSE) xQueueSend(serial_tx_verbose_queue, "\n   Warte auf ACK vom Server", pdMS_TO_TICKS(1));
  while (!client.available()) {
    vTaskDelay(pdMS_TO_TICKS(100));
    if (VERBOSE) xQueueSend(serial_tx_verbose_queue, ".", pdMS_TO_TICKS(1));
  }

  if (VERBOSE) xQueueSend(serial_tx_verbose_queue, "\n   ACK vom Server erhalten:", pdMS_TO_TICKS(1));
  len = client.available();
  ack = new uint8_t[len+1];
  while (client.available()) 
  {
    client.read((uint8_t*)ack, len);
    ack[len] = '\0';
    if (VERBOSE) queue_printf<VERBOSE_BUFFER_SIZE>(serial_tx_verbose_queue, "%s", ack);

  }
  delete[] ack; //next ack might have different length -> delete and allocate new memory

  ///////////////////////////////////////////
  //send request to server and wait for ack
  ///////////////////////////////////////////

  if (VERBOSE) xQueueSend(serial_tx_verbose_queue, "\n   Frage Daten an...", pdMS_TO_TICKS(1));
  client.write(request, request_len);

  if (VERBOSE) xQueueSend(serial_tx_verbose_queue, "\n   Warte auf ACK vom Server", pdMS_TO_TICKS(1));
  while (!client.available()) 
  {
    vTaskDelay(pdMS_TO_TICKS(500));
    if (VERBOSE) xQueueSend(serial_tx_verbose_queue, ".", pdMS_TO_TICKS(1));
  }

  if (VERBOSE) xQueueSend(serial_tx_verbose_queue, "\n   ACK vom Server erhalten:", pdMS_TO_TICKS(1));
  
  len = client.available();
  ack = new uint8_t[len+1];
  
  while (client.available()) 
  {
    client.read((uint8_t*)ack, len);
    ack[len] = '\0';
    if (VERBOSE) queue_printf<VERBOSE_BUFFER_SIZE>(serial_tx_verbose_queue, "%s", ack);
  }
  delete[] ack;

  //////////////////////////////////////////////
  //send request_end to server and wait for ack
  //////////////////////////////////////////////

  if (VERBOSE) xQueueSend(serial_tx_verbose_queue, "\n   Sende request_end", pdMS_TO_TICKS(1));
  client.write(request_end, request_end_len);

  if (VERBOSE) xQueueSend(serial_tx_verbose_queue, "\n   Warte auf ACK vom Server", pdMS_TO_TICKS(1));
  
  while (!client.available()) 
  {
    vTaskDelay(pdMS_TO_TICKS(500));
    if (VERBOSE) xQueueSend(serial_tx_verbose_queue, ".", pdMS_TO_TICKS(1));
  }
  
  if (VERBOSE) xQueueSend(serial_tx_verbose_queue, "\n   ACK vom Server erhalten:", pdMS_TO_TICKS(1));
  
  len = client.available();
  ack = new uint8_t[len+1];
  
  while (client.available()) 
  {
    client.read((uint8_t*)ack, len);
    ack[len] = '\0';
    if (VERBOSE) queue_printf<VERBOSE_BUFFER_SIZE>(serial_tx_verbose_queue, "%s", ack);
  }
  delete[] ack;

  ///////////////////////////////////////////
  
  /*
    in the arduino example from the Zusi 2 forum, after client.write(request_end, request_end_len):
    read first paket and ignore payload, then proceed to read actual data 
  */
  //first packet with ignored payload
  uint8_t c[4];
  client.readBytes(c, sizeof(c));  //first 4 bytes indicates payload size
          
  size_t payload_len = 0x0; 
  //little endian: first byte is least significant
  payload_len |= c[0];
  payload_len |= c[1] << 8; 
  payload_len |= c[2] << 16; 
  payload_len |= c[3] << 24; 
                    
  for(int i = 0; i < payload_len; i ++) { client.read(); }
  
  //server sends another packet that 
  //will be handled in the tcp_rx_task

}

//===============================
//(re)connect to W5500 module
//===============================

void start_w5500() 
{
    ESP32_W5500_onEvent();
    ETH.begin( MISO_GPIO, MOSI_GPIO, SCK_GPIO, CS_GPIO, INT_GPIO, SPI_CLOCK_MHZ, ETH_SPI_HOST, mac );
    ESP32_W5500_waitForConnect();
}

//========================================
//send keyreport from mcp_list/pcf_list
//========================================

void send_keyReport(uint8_t ic_address, uint8_t pin) 
{
  /* structure of a keyreport: 
  typedef struct {
    uint8_t modifiers;
    uint8_t reserved;
    uint8_t keys[6];
  } KeyReport;
  */
  char *token; 
  KeyReport *keyReport;
  memset(&keyReport[0], 0, sizeof(KeyReport));  //clear keyreport
  if (ic_address == 255 || pin == 255) //send empty report (aka no key pressed)
  {
    if (!VERBOSE) xQueueSend(keyreport_tx_queue, &keyReport, pdMS_TO_TICKS(1));
    else { 
      queue_printf<VERBOSE_BUFFER_SIZE>(serial_tx_verbose_queue, "\n  Keyreport: %s", (*keyReport).keys);
      queue_printf<VERBOSE_BUFFER_SIZE>(serial_tx_verbose_queue, "\n  Modifiers: %u", (*keyReport).modifiers); //todo: send modifier variable in binary format
    }
    return;
  }
  
  //in address there might be something like "ctrl+a" or "ctrl+shift+a" or "a" -> split it and send the keys in the correct order
  
  if (ic_address - MCP_I2C_BASE_ADDRESS < 10) token = strtok(mcp_list[ic_address].address[pin], "+"); //i2c adress belongs to an mcp ic
  else                                        token = strtok(pcf_list[ic_address].address[pin], "+"); //i2c adress belongs to a pcf ic
  
  while (token != NULL) 
  {
    if (strcmp(token, "ctrl") == 0) (*keyReport).modifiers |= KEY_LEFT_CTRL; // "|=" to combine multiple modifier keys instead of overwriting
    else if (strcmp(token, "shift") == 0) (*keyReport).modifiers |= KEY_LEFT_SHIFT;
    else if (strcmp(token, "alt") == 0) (*keyReport).modifiers |= KEY_LEFT_ALT;
    else if (strcmp(token, "win") == 0) (*keyReport).modifiers |= KEY_LEFT_GUI;
    else if (strcmp(token, "arrow_left") == 0) (*keyReport).modifiers |= KEY_LEFT_ARROW;
    else if (strcmp(token, "arrow_right") == 0) (*keyReport).modifiers |= KEY_RIGHT_ARROW;
    else if (strcmp(token, "arrow_up") == 0) (*keyReport).modifiers |= KEY_UP_ARROW;
    else if (strcmp(token, "arrow_down") == 0) (*keyReport).modifiers |= KEY_DOWN_ARROW;
    else 
    {  
      if ((*keyReport).keys[0] == 0) (*keyReport).keys[0] = token[0]; 
      else if ((*keyReport).keys[1] == 0) (*keyReport).keys[1] = token[0];
      else if ((*keyReport).keys[2] == 0) (*keyReport).keys[2] = token[0];
      else if ((*keyReport).keys[3] == 0) (*keyReport).keys[3] = token[0];
      else if ((*keyReport).keys[4] == 0) (*keyReport).keys[4] = token[0];
      else if ((*keyReport).keys[5] == 0) (*keyReport).keys[5] = token[0]; //up to 6 keys can be pressed at once
    }
    token = strtok(NULL, "+");
  }
  if (!VERBOSE) xQueueSend(keyreport_tx_queue, &keyReport, pdMS_TO_TICKS(1));
  else { queue_printf<VERBOSE_BUFFER_SIZE>(serial_tx_verbose_queue, "\n  Keyreport: %s", (*keyReport).keys); }

  //TODO: check if differentiating between switch and button is necessary
  /*
    //button handling - wait a bit before "releasing" the keys:
    
    vTaskDelay(pdMS_TO_TICKS(30));
    //clear keyreport and send to queue
    memset(keyReport, 0, sizeof(KeyReport));
    if (!VERBOSE) xQueueSend(keyreport_tx_queue, &keyReport, pdMS_TO_TICKS(1));
    else { queue_printf<VERBOSE_BUFFER_SIZE>(serial_tx_verbose_queue, "\n  Keyreport: %s", (*keyReport).keys); }
  */
}

//=========================================================================================
//polls every connected MCP IC and checks for changes of the input pin states. 
//if a pin state changed, a command is created and added to the queue "keyreport_tx_queue"
//=========================================================================================

void digital_input_task (void * pvParameters)
{ 
  uint16_t readingAB = 0b0;
  uint16_t ab_flag = 0b0;     //set bit indicates, which pin of a port changed 
  
  char verbose_buffer[VERBOSE_BUFFER_SIZE] = {'\0'};
  char data[5];                         //4 chars + nullterminator
  uint8_t t, i, j;                      //local for-loop counter

  TickType_t xLastWakeTime= xTaskGetTickCount();     //used for debounce delay

  for(;;)
  {
    vTaskDelay(5);
      
    for (i = 0; i < MAX_IC_COUNT; i++) //loop through every connected IC
    {
      if(!mcp_list[i].enabled) continue; //IC with this i2c address is not enabled

      if (xSemaphoreTake(i2c_mutex, portMAX_DELAY) == pdTRUE) { 
        readingAB = mcp_list[i].mcp.read(); //read gpio register of IC
        xSemaphoreGive(i2c_mutex);
      }
      
      ab_flag = (readingAB ^ mcp_list[i].last_reading) & mcp_list[i].portMode; 
 
      if (!ab_flag) continue; //check if some input-pin state changed. If not, continue and check next IC
         
      xTaskDelayUntil( &xLastWakeTime, pdMS_TO_TICKS(DEBOUNCE_DELAY_MS) );  //suspend this task and free cpu resources for the specific debounce delay
        
      if (xSemaphoreTake(i2c_mutex, portMAX_DELAY) == pdTRUE) {
        readingAB = mcp_list[i].mcp.read(); //read gpio register of IC again
        xSemaphoreGive(i2c_mutex);
      }

      ab_flag = (readingAB ^ mcp_list[i].last_reading) & mcp_list[i].portMode;   //check again for state-changes of an input-pin 

      if (!ab_flag) continue;           //check if pin-state is still the same after debounce delay
             
      mcp_list[i].last_reading = readingAB;     //update with new reading

      if(VERBOSE) 
      {
        char *port = new char[17];
        memset(port, '\0', 17);
        for (j = 0; j < 16; j++) { strcat(port, (CHECK_BIT(readingAB, j) ? "1" : "0")); }
        queue_printf<VERBOSE_BUFFER_SIZE>(serial_tx_verbose_queue, "\n[digital_input_task]\n  GPIO Register: %s\n", port);
      }

      //========================================================================================================
      /*
      * these pins are used in analog_input_task, to indicate direction of the throttle / potentiometer
      * -> dont transmit keyreport, if one of these pins changed its state
      * only update the status of the variable acceleration_button_status / deceleration_button_status 
      */
      if(i == acceleration_button[0] - MCP_I2C_BASE_ADDRESS) 
      {
        acceleration_button_status = CHECK_BIT(readingAB, acceleration_button[1]) == 0; //check if button was pressed/released
        if (acceleration_button_status) continue;
      }
      if (i == deceleration_button[0] - MCP_I2C_BASE_ADDRESS) 
      {
        deceleration_button_status = CHECK_BIT(readingAB, deceleration_button[1]) == 0;
        if (deceleration_button_status) continue;
      }
      //========================================================================================================
      //loop through every bit of ab_flag (every pin of every connected IC) an look for the index of the set bit
      for (t = 0; t < 16; t++)   
      {                                         
        if (!CHECK_BIT(ab_flag, t)) continue;      //continue, if bit == 0
        if(i == acceleration_button[0] - MCP_I2C_BASE_ADDRESS && t == acceleration_button[1]) continue; 
        if(i == deceleration_button[0] - MCP_I2C_BASE_ADDRESS && t == deceleration_button[1]) continue; 
      
        if (VERBOSE) 
        {
          snprintf(verbose_buffer, VERBOSE_BUFFER_SIZE,
            "\n[digital_input_task]\n  Pin: %c%d\n  I2C-Adresse (DEC): %d\n",
            (t > 7) ? 'B' : 'A', 
            (t > 7) ? t - 8 : t,
            i + MCP_I2C_BASE_ADDRESS
          );
          xQueueSend(serial_tx_verbose_queue, &verbose_buffer, pdMS_TO_TICKS(1));
        }

        //difference between button and switch unclear
        send_keyReport(i, t); //send keyReport according to the pin that changed its state and send to queue
        vTaskDelay(50);
        send_keyReport(255, 255); //255 to release all keys
        
      }   
    }

  } 
}

//================================================================================================
//similar to digital_input_task. 
//difference is that the connected PCF ICs are responsible for AD conversion and constantly polled
//================================================================================================

void analog_input_task (void * pvParameters) //needs 
{
  
  char verbose_buffer[VERBOSE_BUFFER_SIZE] = {'\0'}; 
  char data[5];
  uint8_t ic_address, pin;

  uint8_t reading[4];           //every pcf-IC has 4 analog inputs
  uint8_t t, i, j;              //local for-loop counter

  for(;;)
  {
    vTaskDelay(5);
    
    for (i = 0; i < MAX_IC_COUNT; i++) 
    {
      if(!pcf_list[i].enabled) continue;//i2c adress not in use   

      for (int t = 0; t < 4; t++)   // read every analog Input
      { 
        if (xSemaphoreTake(i2c_mutex, portMAX_DELAY) == pdTRUE) { 
          reading[t] = pcf_list[i].pcf.analogRead(t);
          xSemaphoreGive(i2c_mutex);
        }                                              
        
        if (reading[t] > pcf_list[i].last_reading[t] + ADC_STEP_THRESHOLD || reading[t] < pcf_list[i].last_reading[t] - ADC_STEP_THRESHOLD)// || reading[t] <= ADC_STEP_THRESHOLD || reading[t] >= 255-ADC_STEP_THRESHOLD ) //filter out inaccuracy of mechanics and the ADC
        {
          if(VERBOSE) queue_printf<VERBOSE_BUFFER_SIZE>(serial_tx_verbose_queue, "\n[analog input]\n  Wert an Pin %i von PCF-IC mit Adresse %i: %u\n", t, i+72, reading[t]);
          
          if (reading[t] <= ADC_STEP_THRESHOLD) reading[t] = 0;
          else if (reading[t] >= 255 - ADC_STEP_THRESHOLD) reading[t] = 255;

          pcf_list[i].last_reading[t] = reading[t];

          if ((i + PCF_I2C_BASE_ADDRESS) == combined_throttle_ic[0] && t == combined_throttle_ic[1]) //check if throttle has been moved
          {  
            if (acceleration_button_status) 
            {
              Serial.println("  beschleunigen");
              ic_address = acceleration_button[0];
              pin = acceleration_button[1];
            }
            else if (deceleration_button_status) 
            {
              Serial.println("  bremsen");
              ic_address = deceleration_button[0];
              pin = deceleration_button[1];
            }
            else if (VERBOSE) queue_printf<VERBOSE_BUFFER_SIZE>(serial_tx_verbose_queue, "\n[analog input]\n  Kombihebel betätigt. Richtung kann aber nicht ermittelt werden.\n");            
          } 
          else {
            ic_address = i + PCF_I2C_BASE_ADDRESS;
            pin = t; 
          }           
         
          send_keyReport(ic_address, pin); //send keyReport according to the pin that changed its state and send to queue
          vTaskDelay(50);
          send_keyReport(255, 255); //255 to release all keys

          if(VERBOSE) 
          {
            memset(&verbose_buffer[0], '\0', CMD_BUFFER_SIZE);              //clear cmd char array

            strcat(verbose_buffer, "\n[analog input]\n  I2C-Adresse (DEC): ");
            strcat(verbose_buffer, String(i+PCF_I2C_BASE_ADDRESS).c_str());
            strcpy(verbose_buffer, "  Pin: ");
            strcat(verbose_buffer, String(t).c_str());
            strcat(verbose_buffer, "\n");

            xQueueSend(serial_tx_verbose_queue, &verbose_buffer, pdMS_TO_TICKS(1));
          }                                           
        }
      }
    }

  } 
}

//======================================================
//set outputs according to received data from tcp-server
//======================================================

void output_task(void * pvParameters)
{  

  tcp_payload payload;
  uint8_t t, i;                    //local for-loop counter
  float fvalue;
  int ivalue;

  for(;;)
  {
    vTaskDelay(5);
    xQueueReceive(tcp_rx_queue, &payload, portMAX_DELAY); //block this task, if queue is empty

    for (t = 0; t < payload.count; t++) //loop through every command in the received data-packet
    {
      if (VERBOSE) queue_printf<VERBOSE_BUFFER_SIZE>(serial_tx_verbose_queue, "\n[output task]\n  TCP Datenpaket empfangen: %s\n", payload.message);

      //first two bytes are headers (indicate if a value changed), 5 bytes for each command
      //first byte is the command, second byte is the value
      //commands are defined in lokSim3D_interface.h
      for (i = 0; i < payload.count; i++) 
      {
        int command_index = 2+i*5; //command is at index 2, 7, 12, 17, ... (1 byte)
        int data_index = 3+i*5;   //data is at index 3, 8, 13, 18, ... (4 bytes)
        if (payload.message[command_index] == GESCHWINDIGKEIT) 
        {
          
          //convert payload.message[data_index] to float: 
          memcpy(&fvalue, &payload.message[data_index], sizeof(fvalue));
          //convert float to int and m/s to km/h:
          ivalue = static_cast<int>(fvalue * 3.6);
          //map value to 0-255:
          ivalue = map(ivalue, 0, 90, 0, 255); //0-90 km/h

          analogWrite(GESCHWINDIGKEIT, fvalue);
          if (VERBOSE) queue_printf<VERBOSE_BUFFER_SIZE>(serial_tx_verbose_queue, "\n[output task]\n  Geschwindigkeit: %u\n", fvalue);
        }

        if(VERBOSE) queue_printf<VERBOSE_BUFFER_SIZE>(serial_tx_verbose_queue, "\n[output task]\n  TCP Datenpaket empfangen: %s\n", payload.message);
      }
      delete[] payload.message; //free memory
    } 
  }
}

//==================================================================
//receive data and add to according queues - should not be suspended
//==================================================================

void rx_task (void * pvParameters) 
{
  uint8_t t, i;  
  tcp_payload payload;

  TickType_t update_interval = pdMS_TO_TICKS(60); 
  TickType_t last_update = xTaskGetTickCount();      //xTaskGetTickCount -> returns the number of ticks since the scheduler started    

  TickType_t request_timeout = pdMS_TO_TICKS(5000); // 5 seconds timeout
  TickType_t request_time = xTaskGetTickCount();

  for(;;)
  {
    vTaskDelay(5);

    if (Serial.available() > 0) 
    {
      if (Serial.peek() == 'M' || Serial.peek() == 'm')  vTaskResume(config_task_handle);
    }

    //check if connection with w5500-IC is still established and if client is connected 
    if (ESP32_W5500_eth_connected && client.connected() && xTaskGetTickCount() - last_update > update_interval) 
    {
      last_update = xTaskGetTickCount();
      
      //wait for data from loksim3d server - if no data is received after request_timeout, client will disconnect and try to reconnect
      while (!client.available() && (xTaskGetTickCount() - request_time) < request_timeout) { 
          vTaskDelay(update_interval); 
      }

      //not sure, if loksim3d uses zusi 2 or 3 protocol
      //this is the zusi 2 protocol
      if (client.available()) 
      {
          uint8_t c[4];
          client.readBytes(c, sizeof(c));  //first 4 bytes indicates payload size
          
          size_t payload_len = 0x0;
          payload_len |= c[0];
          payload_len |= c[1] << 8; 
          payload_len |= c[2] << 16; 
          payload_len |= c[3] << 24;
          
          char pld[payload_len];
          for(i = 0; i < payload_len; i ++) pld[i] = client.read(); 

          
          if (payload_len > 3) 
          { 
            //received new data for the requested data-sets
            payload.message = new char[payload_len];
            memcpy(payload.message, pld, payload_len);  //copy received data to payload.message (tcp_payload struct)

            int count = (payload_len - 2) / 5; //first two bytes are headers (indicate if a value changed), 5 bytes for each command
            payload.count = count;              
            
            xQueueSend(tcp_rx_queue, &payload, pdMS_TO_TICKS(1)); //put struct into tcp_rx_queue
          }
      } //server didn't send any data -> try to reconnect
      else 
      {   
        if (VERBOSE) queue_printf<VERBOSE_BUFFER_SIZE>(serial_tx_verbose_queue, "\n[rx_task]\n  Timeout: Keine Daten vom LOKSIM3D-Server empfangen.\nClient wird neu verbunden.\n");
        if (!ESP32_W5500_eth_connected) //check if connection with w5500-IC is still established
        {
          start_w5500(); //reconnect to w5500 module
        }
        //then try to reconnect to LOKSIM3D server
        client.stop();
        while (!client.connect(host, port, 5000)) //try to connect to LOKSIM3D server with 5 seconds timeout
        { 
          Serial.print(F("."));
          vTaskDelay(2000);
        }
        handshake_and_request(); //if connection was successful, send handshake and request data
        
      }
    }

  }
}

//===================================
//transmit data from queues
//===================================
void tx_task (void * pvParameters) 
{ 

  KeyReport keyReport;

  char buffer[VERBOSE_BUFFER_SIZE]; //only one buffer -> overwrite content after Serial.print

  for(;;)
  { 
    vTaskDelay(5);

    if (xQueueReceive(keyreport_tx_queue, &keyReport, 0) == pdTRUE)    Keyboard.sendReport(&keyReport);
     
    if (VERBOSE && xQueueReceive(serial_tx_verbose_queue, &buffer, 0) == pdTRUE)     Serial.print(buffer);
        
  }
}

//=================================================================================
//configuration menu
//extra task to be able to suspend and resume other tasks from within this function
//==================================================================================

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

//================================================================================================================
//================================================================================================================

void init_interface() 
  {
    Keyboard.begin();
    Wire.begin();       //initialize i2c bus
    if (!load_config()) return; //load config from files
    /*
    //test Keyboard output
    Keyboard.press('M');  
    vTaskDelay(1000); 
    Keyboard.releaseAll();
    */

    //store received data from tcp server in form of a tcp_payload struct
    tcp_rx_queue = xQueueCreate(4, sizeof(tcp_payload));        
    //stores commands, that are created in both input-tasks and then transmitted by the tx task 
    keyreport_tx_queue = xQueueCreate(4, sizeof(KeyReport));   

    serial_tx_verbose_queue = xQueueCreate(4, VERBOSE_BUFFER_SIZE*sizeof(char)); //stores i2c address and MCP Pin, if run_config_task==true      
    
    i2c_mutex = xSemaphoreCreateMutex();
    xTaskCreate(digital_input_task, "Task1", 2000, NULL, 1, &digital_input_task_handle);
    xTaskCreate(analog_input_task, "Task2", 2000, NULL, 1, &analog_input_task_handle);
    xTaskCreate(output_task, "Task3", 2000, NULL, 1, &output_task_handle);
    xTaskCreate(
      rx_task,              /* Task function. */
      "Task4",              /* name of the task. */
      2000,                 /* Stack size */
      NULL,                 /* optional parameters*/
      1,                    /* priority */
      &rx_task_handle               /* Task handle to keep track of created task */
    );
    xTaskCreate(tx_task, "Task5", 2000, NULL, 1, &tx_task_handle);
    xTaskCreate(config_task, "Task6", 8000, NULL, 1, &config_task_handle);

    //suspend tasks before trying to connect to LOKSIM3D server
    //dont suspend task 5 to allow user to enter the config menu
    vTaskSuspend(config_task_handle);
    vTaskSuspend(digital_input_task_handle);
    vTaskSuspend(analog_input_task_handle);
    vTaskSuspend(output_task_handle);

    Serial.print(F("\nTasks erfolgreich initialisiert.\nUm in das Konfigurationsmenü zu gelangen, \"M\" eingeben.\n"));

    start_w5500(); //connect to w5500 module

    Serial.print(F("\nVerbindung zu LOKSIM3D TCP-Server wird hergestellt...\n"));

    while (!client.connect(host, port, 5000)) { //try to connect to LOKSIM3D server with 5 seconds timeout
      Serial.print(F("."));
      vTaskDelay(2000);
    }
    
    Serial.print(F("\nVerbindung erfolgreich hergestellt.\n"));

    //send handshake and request data - called one time after connection is established
    handshake_and_request();       

    //resume tasks after successful connection
    vTaskResume(digital_input_task_handle);
    vTaskResume(analog_input_task_handle);
    vTaskResume(output_task_handle);

  }

} //namespace lokSim3D_interface

#endif