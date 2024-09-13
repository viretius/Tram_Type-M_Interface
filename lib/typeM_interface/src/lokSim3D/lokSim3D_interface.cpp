#include <lokSim3D/lokSim3D_interface.h>

using namespace lokSim3D_config;

namespace lokSim3D_interface {

//========================================================================================================
//pwm output for buzzer not used yet
//========================================================================================================

static void toggle_buzzer(uint8_t pin, float frequency) { //frequency of 0 turns off the buzzer
  ledcSetup(0, frequency, 12); 
  ledcAttachPin(pin, 0);
  ledcWriteTone(0, frequency);
}
//========================================================================================================
//client handshake with Zusi 2(!) server 
//========================================================================================================

void handshake_and_request() {

  size_t len;
  uint8_t *ack;
  
  if (VERBOSE) xQueueSend(serial_tx_verbose_queue, "\nHandshake\n   Handshake wird durchgeführt\n", pdMS_TO_TICKS(1));
  client.write(handshake, handshake_len);
  if (VERBOSE) xQueueSend(serial_tx_verbose_queue, "\n   Warte auf ACK vom Server", pdMS_TO_TICKS(1));
  while (!client.available()) {
    vTaskDelay(pdMS_TO_TICKS(500));
    queue_printf<VERBOSE_BUFFER_SIZE>(serial_tx_verbose_queue, ".");
  }

  if (VERBOSE) xQueueSend(serial_tx_verbose_queue, "\n   ACK vom Server erhalten:", pdMS_TO_TICKS(1));
  len = client.available();
  ack = new uint8_t[len+1];
  while (client.available()) 
  {
    client.read((uint8_t*)ack, len);
    ack[len] = '\0';
    if (VERBOSE)  queue_printf<VERBOSE_BUFFER_SIZE>(serial_tx_verbose_queue, "%s", ack);
    /*if (length!= 1 || hello_ack.getId() != Cmd_ACK_HELLO)
		{
			throw std::runtime_error("Protocol error - invalid response from server");
		}*/
  }
  delete[] ack; //next ack might have different length -> delete and allocate new memory

  if (VERBOSE) xQueueSend(serial_tx_verbose_queue, "\n   Frage Daten an...", pdMS_TO_TICKS(1));
  client.write(request, REQUEST_LEN);

  if (VERBOSE) xQueueSend(serial_tx_verbose_queue, "\n   Warte auf ACK vom Server", pdMS_TO_TICKS(1));
  while (!client.available()) {
    vTaskDelay(pdMS_TO_TICKS(500));
    if (VERBOSE) queue_printf<VERBOSE_BUFFER_SIZE>(serial_tx_verbose_queue, ".");
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

  client.write(request_end, request_end_len);

  len = client.available();
  ack = new uint8_t[len+1];
  while (client.available()) 
  {
    client.read((uint8_t*)ack, len);
    ack[len] = '\0';
    if (VERBOSE) queue_printf<VERBOSE_BUFFER_SIZE>(serial_tx_verbose_queue, "%s", ack);
  }
  delete[] ack;
  
  /*
    in the arduino example from the Zusi 2 forum, after client.write(request_end, request_end_len):
    read first paket and ignore payload, proceed to read actual data 
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
                    
  char pld[payload_len];
  for(int i = 0; i < payload_len; i ++) { pld[i] = client.read(); }
  
  //server sends another packet
  //will be handled in the tcp_rx_task

}

//========================================================================================================
//(re)connect to W5500 module
//========================================================================================================

void start_w5500() 
{
    ESP32_W5500_onEvent();
    ETH.begin( MISO_GPIO, MOSI_GPIO, SCK_GPIO, CS_GPIO, INT_GPIO, SPI_CLOCK_MHZ, ETH_SPI_HOST, mac );
    ESP32_W5500_waitForConnect();
}

//========================================================================================================
//send keyreport, get keyreport from mcp_list/pcf_list
//========================================================================================================

void send_keyReport(uint8_t ic_address, uint8_t pin) 
{
  /* structure of a keyreport
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
  
  uint8_t key_count = 6; //max 6 simultenous keys / report 

  while (token != NULL && key_count > 0) 
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
      key_count--;
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

//========================================================================================================
//polls every connected MCP IC and checks for changes of the input pin states. 
//if a pin state changed, a command is created and added to the queue "keyreport_tx_queue"
//========================================================================================================

void digital_input_task (void * pvParameters)
{ 
  uint16_t readingAB = 0b0;
  uint16_t ab_flag = 0b0;     //set bit indicates, which pin of a port changed 
  
  char verbose_buffer[VERBOSE_BUFFER_SIZE] = {'\0'};
  char data[5];                         //4 chars + nullterminator
  uint8_t t, i, j;                      //lokal for-loop counter

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

      for (t = 0; t < 16; t++)            //loop through every bit of ab_flag (every pin of every connected IC) an look for the index of the set bit
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

//================================================================================================================
//similar to digital_input_task. difference is that the connected PCF ICs are responsible for AD conversion and constantly polled
//================================================================================================================  

void analog_input_task (void * pvParameters) //needs 
{
  
  char verbose_buffer[VERBOSE_BUFFER_SIZE] = {'\0'}; 
  char data[5];
  uint8_t ic_address, pin;

  uint8_t reading[4];           //every pcf-IC has 4 analog inputs
  uint8_t t, i, j;              //lokal for-loop counter

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
              USBSerial.println("  beschleunigen");
              ic_address = acceleration_button[0];
              pin = acceleration_button[1];
            }
            else if (deceleration_button_status) 
            {
              USBSerial.println("  bremsen");
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

//================================================================================================================
//set outputs according to received data from tcp-server
//================================================================================================================

void output_task(void * pvParameters)
{  

  tcp_payload payload;
  
  uint8_t t, i;                    //lokal for-loop counter
  uint8_t value; //variable to hold from char to hex converted value for an analog output

  for(;;)
  {
    vTaskDelay(5);
    xQueueReceive(tcp_rx_queue, &payload, portMAX_DELAY); //block this task, if queue is empty
    
    if(VERBOSE) queue_printf<VERBOSE_BUFFER_SIZE>(serial_tx_verbose_queue, "\n[output task]\n  TCP Datenpaket empfangen: %s\n", payload.message);

  } 
}

//=================================================================================================================
//receive data and add to according queues - should not be suspended
//=================================================================================================================

void rx_task (void * pvParameters) 
{
  uint8_t t, i;  
  tcp_payload payload;

  TickType_t update_interval = pdMS_TO_TICKS(60); 
  TickType_t last_update = xTaskGetTickCount();       

  TickType_t request_timeout = pdMS_TO_TICKS(5000); // 5 seconds timeout
  TickType_t request_time = xTaskGetTickCount();

  for(;;)
  {
    vTaskDelay(5);

    if (USBSerial.available() > 0) 
    {
      if (USBSerial.peek() == 'M' || USBSerial.peek() == 'm')  vTaskResume(Task6);
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
          for(i = 0; i < payload_len; i ++) { pld[i] = client.read(); }

          int count = (payload_len - 2) / 5; //first two bytes are headers (indicate if a value changed), 5 bytes for each command

          if (count > 2)                    //did any of the requested values change?
          {
            if (payload_len > 3)             //have we received any data?
            {
              payload.message = new char[payload_len];
              memcpy(payload.message, pld, payload_len);
              payload.count = count;              
              xQueueSend(tcp_rx_queue, &payload, pdMS_TO_TICKS(1));
            }
          }
      } //server didn't send any data -> try to reconnect
      else 
      {   
        if (VERBOSE) queue_printf<VERBOSE_BUFFER_SIZE>(serial_tx_verbose_queue, "\n[rx_task]\n  Timeout: Keine Daten vom LOKSIM3D-Server empfangen.\nClient wird neu verbunden.\n");
        if (!ESP32_W5500_eth_connected) //check if connection with w5500-IC is still established
        {
          ETH.~ESP32_W5500();
        }

        client.stop();
        while (!client.connect(host, port, 5000)) //try to connect to LOKSIM3D server with 5 seconds timeout, if connection was lost
        { 
          USBSerial.print(F("."));
          vTaskDelay(2000);
        }
        handshake_and_request(); //if connection was successful, send handshake and request data
        
      }
    }

  }
}

//================================================================================================================
//transmit data from queues
//================================================================================================================
void tx_task (void * pvParameters) 
{ 

  KeyReport keyReport;

  char buffer[VERBOSE_BUFFER_SIZE]; //only one buffer -> overwrite content after USBSerial.print

  for(;;)
  { 
    vTaskDelay(5);

    if (xQueueReceive(keyreport_tx_queue, &keyReport, 0) == pdTRUE)    Keyboard.sendReport(&keyReport);
     
    if (VERBOSE && xQueueReceive(serial_tx_verbose_queue, &buffer, 0) == pdTRUE)     USBSerial.print(buffer);
        
  }
}

//================================================================================================================
//configuration menu
//extra task to be able to suspend and resume other tasks from within this function
//================================================================================================================

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

    serial_config_menu();  //Tasks can also be suspended and resumed from within this function

    if (!run_config_task) 
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

    Keyboard.begin();
    while(!USBSerial);

    if (!load_config()) return;
    
    /*Keyboard.press('M');  //test HID
    vTaskDelay(1000); 
    Keyboard.releaseAll();*/
    
   
    tcp_rx_queue = xQueueCreate(4, sizeof(tcp_payload)); 
    keyreport_tx_queue = xQueueCreate(4, sizeof(KeyReport));   //stores commands, that are created in both input-tasks and then transmitted by the tx task 

    serial_tx_verbose_queue = xQueueCreate(4, VERBOSE_BUFFER_SIZE*sizeof(char)); //stores i2c address and MCP Pin, if run_config_task==true      
    
    i2c_mutex = xSemaphoreCreateMutex();
    xTaskCreate(digital_input_task, "Task1", 2000, NULL, 1, &Task1);
    xTaskCreate(analog_input_task, "Task2", 2000, NULL, 1, &Task2);
    xTaskCreate(output_task, "Task3", 2000, NULL, 1, &Task3);
    xTaskCreate(
      rx_task,              /* Task function. */
      "Task4",              /* name of the task. */
      2000,                 /* Stack size */
      NULL,                 /* optional parameters*/
      1,                    /* priority */
      &Task4               /* Task handle to keep track of created task */
    );
    xTaskCreate(tx_task, "Task5", 2000, NULL, 1, &Task5);
    xTaskCreate(config_task, "Task6", 8000, NULL, 1, &Task6);

    //suspend tasks before trying to connect to LOKSIM3D server
    //dont suspend task 5 to allow user to enter the config menu
    vTaskSuspend(Task6);
    vTaskSuspend(Task1);
    vTaskSuspend(Task2);
    vTaskSuspend(Task3);

    USBSerial.print(F("\nTasks erfolgreich initialisiert.\nUm in das Konfigurationsmenü zu gelangen, \"M\" eingeben.\n"));

    start_w5500(); //connect to w5500 module

    USBSerial.print(F("\nVerbindung zu LOKSIM3D TCP-Server wird hergestellt...\n"));

    while (!client.connect(host, port, 5000)) { //try to connect to LOKSIM3D server with 5 seconds timeout
      USBSerial.print(F("."));
      vTaskDelay(2000);
    }
    
    USBSerial.print(F("\nVerbindung erfolgreich hergestellt.\n"));

    //send handshake and request data - called one time after connection is established
    handshake_and_request();       

    //resume tasks after successful connection
    vTaskResume(Task1);
    vTaskResume(Task2);
    vTaskResume(Task3);

  }

} //namespace lokSim3D_interface
