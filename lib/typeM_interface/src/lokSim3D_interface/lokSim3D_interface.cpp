/*
key report defines:

#define KEY_LEFT_CTRL   0x80
#define KEY_LEFT_SHIFT  0x81
#define KEY_LEFT_ALT    0x82
#define KEY_LEFT_GUI    0x83
#define KEY_RIGHT_CTRL  0x84
#define KEY_RIGHT_SHIFT 0x85
#define KEY_RIGHT_ALT   0x86
#define KEY_RIGHT_GUI   0x87

#define KEY_UP_ARROW    0xDA
#define KEY_DOWN_ARROW  0xD9
#define KEY_LEFT_ARROW  0xD8
#define KEY_RIGHT_ARROW 0xD7
#define KEY_BACKSPACE   0xB2
#define KEY_TAB         0xB3
#define KEY_RETURN      0xB0
#define KEY_ESC         0xB1
#define KEY_INSERT      0xD1
#define KEY_DELETE      0xD4
#define KEY_PAGE_UP     0xD3
#define KEY_PAGE_DOWN   0xD6
#define KEY_HOME        0xD2
#define KEY_END         0xD5
#define KEY_CAPS_LOCK   0xC1

*/
//================================================================================================================
//================================================================================================================


#include <lokSim3D_interface/lokSim3D_interface.h>

using namespace lokSim3D_config;

namespace lokSim3D_interface {


static USBHIDKeyboard Keyboard;
static WiFiClient client;

//========================================================================================================
//pwm output for buzzer
//========================================================================================================

void toggle_buzzer(uint8_t pin, float frequency) {}

//========================================================================================================
//client handshake with loksim3d server
//========================================================================================================
void handshake_and_request() {

  size_t len;
  uint8_t *ack;

  //vTaskSuspend(Task5);
  
  if (VERBOSE) xQueueSend(serial_tx_verbose_queue, "\nHandshake\n   Handshake wird durchgeführt\n", pdMS_TO_TICKS(1));
  client.write(handshakedata, handshakedata_len);
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
    in arduino example:
    after client.write(request_end, request_end_len):
    read first paket and ignore payload whysoever
    proceed to read actual data 
  */
  //first packet with ignored payload
  char c[4];
  client.readBytes(c, sizeof(c));                 //first 4 bytes indicates payload size
          
  unsigned long payload_len = *(unsigned long *)c; //reinterpret the 4 bytes as an unsigned long
          
  char pld[payload_len];
  for(int i = 0; i < payload_len; i ++) { pld[i] = client.read(); }
  
  //server sends another packet
  //payload is beeing processed in output task

  //vTaskResume(Task5);
}


//========================================================================================================
//
//========================================================================================================

void press_and_release_key(uint8_t ic_address, uint8_t pin) 
{
  KeyReport *keyReport;
  memset(&keyReport, 0, sizeof(KeyReport));  
  if (ic_address == nullptr || pin == nullptr) //send empty report (aka no key pressed)
  {
    if (!VERBOSE) xQueueSend(keyboard_tx_queue, &keyReport, pdMS_TO_TICKS(1));
    else { queue_printf<VERBOSE_BUFFER_SIZE>(serial_tx_verbose_queue, "\n  Keyreport: %s", (*keyReport).keys); }
    return;
  }
  
  //in address there might be something like "ctrl+a" or "ctrl+shift+a" or "a" -> split it and send the keys in the correct order
  char *token = strtok(mcp_list[ic_address].address[pin], "+");
  uint8_t key_count = 6; //max 6 simultenous keys / report 

  while (token != NULL && key_count > 0) 
  {
    if (strcmp(token, "ctrl") == 0) (*keyReport).modifiers |= KEY_LEFT_CTRL; // "|=" to combine multiple modifier keys instead of overwriting
    else if (strcmp(token, "shift") == 0) (*keyReport).modifiers |= KEY_LEFT_SHIFT;
    else if (strcmp(token, "alt") == 0) (*keyReport).modifiers |= KEY_LEFT_ALT;
    else if (strcmp(token, "gui") == 0) (*keyReport).modifiers |= KEY_LEFT_GUI;
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
  if (!VERBOSE) xQueueSend(keyboard_tx_queue, &keyReport, pdMS_TO_TICKS(1));
  else { queue_printf<VERBOSE_BUFFER_SIZE>(serial_tx_verbose_queue, "\n  Keyreport: %s", (*keyReport).keys); }

  //wait a bit before "releasing" the keys
  /*vTaskDelay(pdMS_TO_TICKS(30));
  //clear keyreport and send to queue
  memset(keyReport, 0, sizeof(KeyReport));
  if (!VERBOSE) xQueueSend(keyboard_tx_queue, &keyReport, pdMS_TO_TICKS(1));
  else { queue_printf<VERBOSE_BUFFER_SIZE>(serial_tx_verbose_queue, "\n  Keyreport: %s", (*keyReport).keys); }
  */
}



//========================================================================================================


/*
polls every connected MCP IC and checks for changes of the input pin states. Interrupt not used, because only the i2c bus is connected for compatibility reasons
if a pin state changed, a command is created and added to the queue "keyboard_tx_queue"
*/

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
      
    for (i = 0; i < MAX_IC_COUNT; i++) 
    {
      if(!mcp_list[i].enabled) continue; //IC with this i2c address is "deactivated"

      if (xSemaphoreTake(i2c_mutex, portMAX_DELAY) == pdTRUE) { 
        readingAB = mcp_list[i].mcp.read(); //port A: LSB
        xSemaphoreGive(i2c_mutex);
      }
      
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
        
      if (xSemaphoreTake(i2c_mutex, portMAX_DELAY) == pdTRUE) {
        readingAB = mcp_list[i].mcp.read(); //port A: LSB
        xSemaphoreGive(i2c_mutex);
      }

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

      for (t = 0; t < 16; t++)            //loop through every bit of ab_flag (every pin of every connected IC)
      {                                         
        if (!CHECK_BIT(ab_flag, t)) continue;      //Check which specific pin changed its state
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

        //how to differentiate between button / switch ?? another entry in config file??? >:(
        //or is it possible to change settings of the sim?
        //e.g. button for horn and switch for light
        //if switch: send report according to (ab_flag & readingAB) bit operation
          //if true:
          press_and_release_key(i, t); //send keyReport according to the pin that changed its state and send to queue
          //else release key:
          vTaskDelay(50);
          press_and_release_key(nullptr, nullptr);
        //else if button: 
          //press_and_release_key(i, t); //send keyReport according to the pin that changed its state and send to queue
          //vTaskDelay(50);
          //press_and_release_key(nullptr, nullptr);
      }   
    }

  } 
}

/*
similar to digital_input_task. difference is that the connected PCF ICs are responsible for AD conversion and constantly polled
*/

void analog_input_task (void * pvParameters) //needs 
{
  
  char cmd_buffer[CMD_BUFFER_SIZE] = {'\0'}; 
  char verbose_buffer[VERBOSE_BUFFER_SIZE] = {'\0'}; 
  char data[5];

  bool acceleration_button_status = 0;
  bool deceleration_button_status = 0;

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

          sprintf(data, "%04X", reading[t]);    

          memset(&cmd_buffer[0], '\0', CMD_BUFFER_SIZE); //clear cmd char array

          char address[3] = {0,0, 0};
          memset(&address[0], '\0', 3); //clear cmd char array

          if ((i + PCF_I2C_BASE_ADDRESS) == combined_throttle_ic[0] && t == combined_throttle_ic[1]) //&& reading[t] >= ADC_STEP_THRESHOLD && reading[t] <= 25 - ADC_STEP_THRESHOLD) ;
          { 
            if (acceleration_button_status) 
            {
              USBSerial.println("  beschleunigen");
              strcat( address, mcp_list[acceleration_button[0] - MCP_I2C_BASE_ADDRESS].address[acceleration_button[1]] );
            }
            else if (deceleration_button_status) 
            {
              USBSerial.println("  bremsen");
              strcat( address, mcp_list[deceleration_button[0] - MCP_I2C_BASE_ADDRESS].address[deceleration_button[1]] );
            }
            else if (VERBOSE) queue_printf<VERBOSE_BUFFER_SIZE>(serial_tx_verbose_queue, "\n[analog input]\n  Kombihebel betätigt. Richtung kann aber nicht ermittelt werden.\n");
            else continue;
          }
          else strcat(address, pcf_list[i].address[t]); //just any other analog input
          
          //nprintf(cmd_buffer, CMD_BUFFER_SIZE, "XV%02s%sY", address, data);

               
          //if (!VERBOSE && eTaskGetState(Task6) != eRunning) xQueueSend(serial_tx_cmd_queue, &cmd_buffer, pdMS_TO_TICKS(1)); 
          
         //make keyreport...
         //press_and_release_key(keyreport);
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

/*
set outputs according to received data over USBSerial connection
*/

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
    
    //skip the first two bytes (indicate number of bytes? / number of commands?, that have been transmitted)
    int command_index = 2;      
    //command consistst of two chars (a number between 0 and 99), 
    //value_index is being castet and interpreted differently depending on command type
    int value_index = 3;   

    for (i = 0; i < payload.count; i++) 
    {
      if(VERBOSE) queue_printf<VERBOSE_BUFFER_SIZE>(serial_tx_verbose_queue, "\n[output task]\n  Befehl: %s\n  Wert: %s", payload.message[command_index], payload.message[value_index]);

      float value_cast_to_float = *((float *)&payload.message[value_index]); //"Single"
      int value_cast_to_int = *((int *)&payload.message[value_index]);      //"enum" based on value different actions are performed

      int command = atoi(&payload.message[command_index]);

      switch (command) 
      {
        case GESCHWINDIGKEIT: 
            for (t = 0; t < MAX_IC_COUNT; t++)
            {
              if (atoi(pcf_list[t].address[0]) == GESCHWINDIGKEIT) //pcf ics only have 1 analog output
              {
                if (xSemaphoreTake(i2c_mutex, portMAX_DELAY) == pdTRUE) 
                {
                  pcf_list[t].pcf.analogWrite(value_cast_to_float);
                  xSemaphoreGive(i2c_mutex);
                } 
              }
            }
          break;
        case SPANNUNG: 
          break;
        case AFB_SOLL_GESCHWINDIGKEIT: 
            for (t = 0; t < MAX_IC_COUNT; t++)
            {
              if (pcf_list[t].enabled == false) continue;
              if (atoi(pcf_list[t].address[0]) == AFB_SOLL_GESCHWINDIGKEIT) //pcf ics only have 1 analog output
              {
                if (xSemaphoreTake(i2c_mutex, portMAX_DELAY) == pdTRUE) 
                {
                  pcf_list[t].pcf.analogWrite(value_cast_to_float);
                  xSemaphoreGive(i2c_mutex);
                } 
              }
            }
          break;
        case PZB_BEFEHL:
          break;
        case PZB_WACHSAM:
          break;
        case PZB_FREI:
          break;
        case SIFA:
          break;
        case TUEREN: //enum type  
            switch(value_cast_to_int)
            {
              case 0: //türen freigegeben
                for (t = 0; t < MAX_IC_COUNT; t++)
                  {
                    for (i = 0; i < 16; i++)  //mcp ics with 16 I/O
                    {
                      if (mcp_list[t].enabled == false) continue;
                      if (CHECK_BIT(mcp_list[t].portMode, i)) continue; //skip input pins

                      if (atoi(mcp_list[t].address[i]) == TUEREN) //mcp ics have 8 digital outputs
                      {
                        if (xSemaphoreTake(i2c_mutex, portMAX_DELAY) == pdTRUE) 
                        {
                          mcp_list[t].mcp.digitalWrite(i, 1); //turn on lightbulb 
                          xSemaphoreGive(i2c_mutex);
                        } 
                      }
                    }
                  }
                break;
              case 1: //türen offen
                break;
              case 2: //Achtungspfiff oder Durchsage
              break;
              case 3: //Fahrgaeste i. O.)"
                break;
              case 4: //türen schließen
                break;
              case 5: //türen zu
                break;
              case 6: //abfahrauftrag
                break;
              case 7: //türen verriegelt
                break;
              case 8: //ak. freigabesignal
                break;
              default:
                break;
            }
          break;
        
      }

      command_index += 5;
      value_index += 5;
    }

  } 
}

/*
receive data and add to according queues
*/

void rx_task (void * pvParameters) //also usbserial rx task for config menu
{
  char buffer[2]; //receive a single char with serial monitor if available. 

  uint8_t t, i;  
  tcp_payload payload;

  TickType_t request_interval = pdMS_TO_TICKS(60); 
  TickType_t last_request = xTaskGetTickCount();       

  TickType_t request_timeout = pdMS_TO_TICKS(5000); // 5 seconds timeout
  TickType_t request_time = xTaskGetTickCount();

  for(;;)
  {
    vTaskDelay(5);

    if (USBSerial.available() > 0) 
    {
      USBSerial.readBytesUntil('\n', buffer, 2);                            
      if (buffer[0] == 'M' || buffer[0] == 'm')  vTaskResume(Task6);
      vTaskDelay(pdMS_TO_TICKS(100));
    }

    if (ESP32_W5500_eth_connected && client.connected() && xTaskGetTickCount() - last_request > request_interval) 
    {
      last_request = xTaskGetTickCount();
      
      handshake_and_request();  //send request to server

      //wait for
      while (!client.available() && (xTaskGetTickCount() - request_time) < request_timeout) {
          vTaskDelay(request_interval); 
      }

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

          if (count > 2) //did any of the requested values change?
          {
            if (payload_len > 3)               //????????why?????????
            {
              payload.message = new char[payload_len];
              memcpy(payload.message, pld, payload_len);
              payload.count = count;              
              xQueueSend(tcp_rx_queue, &payload, pdMS_TO_TICKS(1));
            }
          }
      } 
      else 
      {   
          if (VERBOSE) queue_printf<VERBOSE_BUFFER_SIZE>(serial_tx_verbose_queue, "\n[rx_task]\n  Timeout: Keine Daten vom LOKSIM3D-Server empfangen.\nClient wird neu verbunden.\n");
          if (ESP32_W5500_eth_connected) 
          {
            client.stop();
            while (!client.connect(host, port, 5000)) { //try to connect to LOKSIM3D server with 5 seconds timeout
              USBSerial.print(F("."));
              vTaskDelay(2000);
            }
            handshake_and_request();
          }
      }
    }

  }
}

/*
transmit data from queues
*/
void tx_task (void * pvParameters) 
{ /*
  typedef struct
  {
    uint8_t modifiers;
    uint8_t reserved;
    uint8_t keys[6];
  } KeyReport;*/
  KeyReport keyReport;

  char buffer[VERBOSE_BUFFER_SIZE]; //only one buffer -> overwrite content after USBSerial.print

  for(;;)
  {
    vTaskDelay(5);

    if (xQueueReceive(keyboard_tx_queue, &keyReport, 0) == pdTRUE)    Keyboard.sendReport(&keyReport);
     
    if (VERBOSE && xQueueReceive(serial_tx_verbose_queue, &buffer, 0) == pdTRUE)     USBSerial.print(buffer);
        
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
    
    /*Keyboard.press('M');  //test
    vTaskDelay(1000); 
    Keyboard.releaseAll();*/
    
    /*
    USBSerial_rx_cm_queue:
    -1 slot with size of an integer, FIFO principal, stores commands "executed" in output_task
    -only for com between tasks. only one command (of size (CMD_BUFFER_SIZE)) large, because the hardware rx buffer of the esp32 is 256 bytes large
    */
    tcp_rx_queue = xQueueCreate(4, sizeof(tcp_payload)); 
    keyboard_tx_queue = xQueueCreate(4, sizeof(KeyReport));   //stores commands, that are created in both input-tasks and then transmitted by the tx task 

    serial_tx_verbose_queue = xQueueCreate(4, VERBOSE_BUFFER_SIZE*sizeof(char)); //stores i2c address and MCP Pin, if run_config_task==true      
    
    i2c_mutex = xSemaphoreCreateMutex();
    xTaskCreate(digital_input_task, "Task1", 2000, NULL, 1, &Task1);
    xTaskCreate(analog_input_task, "Task2", 2000, NULL, 1, &Task2);
    xTaskCreate(output_task, "Task3", 2000, NULL, 1, &Task3);
    xTaskCreate(tx_task, "Task5", 2000, NULL, 1, &Task5);
    //open config menu from within this task
    xTaskCreate(
      rx_task,              /* Task function. */
      "Task4",              /* name of task. */
      2000,                 /* Stack size */
      NULL,                 /* optional parameters*/
      1,                    /* priority */
      &Task4               /* Task handle to keep track of created task */
    );
    xTaskCreate(config_task, "Task6", 8000, NULL, 1, &Task6);
    vTaskSuspend(Task6);
    vTaskSuspend(Task1);
    vTaskSuspend(Task2);
    vTaskSuspend(Task3);
    //vTaskSuspend(Task5);
    USBSerial.print(F("\nTasks erfolgreich initialisiert.\nUm in das Konfigurationsmenü zu gelangen, \"M\" eingeben.\n"));

    // To be called before ETH.begin()
    ESP32_W5500_onEvent();
   
    ETH.begin( MISO_GPIO, MOSI_GPIO, SCK_GPIO, CS_GPIO, INT_GPIO, SPI_CLOCK_MHZ, ETH_SPI_HOST, mac );
    
    ESP32_W5500_waitForConnect();

    USBSerial.print(F("\nVerbindung zu LOKSIM3D TCP-Server wird hergestellt...\n"));

    while (!client.connect(host, port, 5000)) { //try to connect to LOKSIM3D server with 5 seconds timeout
      USBSerial.print(F("."));
      vTaskDelay(2000);
    }
    
    indicate_finished_setup();

    vTaskResume(Task1);
    vTaskResume(Task2);
    vTaskResume(Task3);

  }
} //namespace lokSim3D_interface
