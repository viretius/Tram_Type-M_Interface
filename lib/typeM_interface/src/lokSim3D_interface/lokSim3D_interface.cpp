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

//========================================================================================================
//pwm output for buzzer
//========================================================================================================

void toggle_buzzer(uint8_t pin, float frequency) 
{

}

//========================================================================================================
//client handshake with loksim3d server
//========================================================================================================
void handshake_and_request() {

  size_t len;
  uint8_t *ack;

  //suspend tasks to prevent calls to client object and sending data to queues, that cannot be processed
  vTaskSuspend(Task1);
  vTaskSuspend(Task2);
  vTaskSuspend(Task3);
  vTaskSuspend(Task5);

  xQueueSend(serial_tx_verbose_queue, "\nHandshake\n   Handshake wird durchgeführt\n", pdMS_TO_TICKS(1));
  client.write(handshakedata, handshakedata_len);
  xQueueSend(serial_tx_verbose_queue, "\n   Warte auf ACK vom Server", pdMS_TO_TICKS(1));
  while (!client.available()) {
    vTaskDelay(pdMS_TO_TICKS(500));
    queue_printf(serial_tx_verbose_queue, VERBOSE_BUFFER_SIZE, ".");
  }

  xQueueSend(serial_tx_verbose_queue, "\n   ACK vom Server erhalten:", pdMS_TO_TICKS(1));
  len = client.available();
  ack = new uint8_t[len+1];
  while (client.available()) 
  {
    client.read((uint8_t*)ack, len);
    ack[len] = '\0';
    queue_printf(serial_tx_verbose_queue, VERBOSE_BUFFER_SIZE, "%s", ack);
  }
  delete[] ack; //next ack might have different length -> delete and allocate new memory

  xQueueSend(serial_tx_verbose_queue, "\n   Frage Daten an...", pdMS_TO_TICKS(1));
  client.write(request, REQUEST_LEN);

  xQueueSend(serial_tx_verbose_queue, "\n   Warte auf ACK vom Server", pdMS_TO_TICKS(1));
  while (!client.available()) {
    vTaskDelay(pdMS_TO_TICKS(500));
    queue_printf(serial_tx_verbose_queue, VERBOSE_BUFFER_SIZE, ".");
  }

  xQueueSend(serial_tx_verbose_queue, "\n   ACK vom Server erhalten:", pdMS_TO_TICKS(1));
  len = client.available();
  ack = new uint8_t[len+1];
  while (client.available()) 
  {
    client.read((uint8_t*)ack, len);
    ack[len] = '\0';
    queue_printf(serial_tx_verbose_queue, VERBOSE_BUFFER_SIZE, "%s", ack);
  }
  delete[] ack;

  vTaskResume(Task1);
  vTaskResume(Task2);
  vTaskResume(Task3);
  vTaskResume(Task5);
}


//========================================================================================================
//
//========================================================================================================

void press_and_release_key(KeyReport *keyReport, uint8_t ic_address, uint8_t pin) 
{
  //in address there might be something like "ctrl+a" or "ctrl+shift+a" or "a" -> split it and send the keys in the correct order
  char *token = strtok(mcp_list[ic_address].address[pin], "+");
  while (token != NULL) 
  {
    if (strcmp(token, "ctrl") == 0) (*keyReport).modifiers |= KEY_LEFT_CTRL; // "|=" to combine multiple modifier keys instead of overwriting
    else if (strcmp(token, "shift") == 0) (*keyReport).modifiers |= KEY_LEFT_SHIFT;
    else if (strcmp(token, "alt") == 0) (*keyReport).modifiers |= KEY_LEFT_ALT;
    else if (strcmp(token, "gui") == 0) (*keyReport).modifiers |= KEY_LEFT_GUI;
    else 
    {  
      if ((*keyReport).keys[0] == 0) (*keyReport).keys[0] = (uint8_t)token[0]; 
      else if ((*keyReport).keys[1] == 0) (*keyReport).keys[1] = (uint8_t)token[0];
      else if ((*keyReport).keys[2] == 0) (*keyReport).keys[2] = (uint8_t)token[0];
      else if ((*keyReport).keys[3] == 0) (*keyReport).keys[3] = (uint8_t)token[0];
      else if ((*keyReport).keys[4] == 0) (*keyReport).keys[4] = (uint8_t)token[0];
      else if ((*keyReport).keys[5] == 0) (*keyReport).keys[5] = (uint8_t)token[0]; //up to 6 keys can be pressed at once
    }
    token = strtok(NULL, "+");
  }
  if (eTaskGetState(Task6) != eRunning) xQueueSend(keyboard_tx_queue, &keyReport, pdMS_TO_TICKS(1));
  if (VERBOSE) queue_printf(serial_tx_verbose_queue, VERBOSE_BUFFER_SIZE, "\n  Keyreport: %s", (*keyReport).keys);

  //wait a bit before "releasing" the keys
  vTaskDelay(pdMS_TO_TICKS(30);
  //clear keyreport and send to queue
  memset(keyReport, 0, sizeof(KeyReport));
  if (eTaskGetState(Task6) != eRunning) xQueueSend(keyboard_tx_queue, &keyReport, pdMS_TO_TICKS(1));
  if (VERBOSE) queue_printf(serial_tx_verbose_queue, VERBOSE_BUFFER_SIZE, "\n  Keyreport: %s", (*keyReport).keys);

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
  
  KeyReport keyReport;  
  char info_buffer[INFO_BUFFER_SIZE] = {'\0'};
  char data[5];                         //4 chars + nullterminator
  uint8_t t, i, j;                      //lokal for-loop counter

  TickType_t xLastWakeTime= xTaskGetTickCount();     //used for debounce delay

  for(;;)
  {
   vTaskDelay(5); //run this task only every 10ms pr so 
   //eTaskGetState(Task6) = eTaskGetState(Task6);

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
      if (  
            ( 
              i == acceleration_button[0] - MCP_I2C_BASE_ADDRESS || 
              i == deceleration_button[0] - MCP_I2C_BASE_ADDRESS 
            ) &&
            ( 
              CHECK_BIT(ab_flag, acceleration_button[1]) ||
              CHECK_BIT(ab_flag, deceleration_button[1]) 
            ) 
          ) continue; 
       
      
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
        queue_printf(serial_tx_verbose_queue, VERBOSE_BUFFER_SIZE, "\n[digital_input_task]\n  GPIO Register: %s\n", port);
      }
          
      for (t = 0; t < 16; t++)            //loop through every bit of ab_flag (every pin of every connected IC)
      {                                         
        if (!CHECK_BIT(ab_flag, t)) continue;      //Check which specific pin changed its state
      
          memset(&keyReport, 0, sizeof(KeyReport));                                  
          press_and_release_key(&keyReport, i, t); //set keyReport according to the pin that changed its state and send to queue
      }             
        
      if(eTaskGetState(Task6) == eRunning) //user 
      {
        memset(&info_buffer[0], '\0', INFO_BUFFER_SIZE);              //clear cmd char array
        strcpy(info_buffer, "\n[digital_input_task]\n  Pin: ");
          
        if (t>7) {
          strcat(info_buffer, "B");
          strcat(info_buffer, String(t-8).c_str());
        }
        else {
          strcat(info_buffer, "A");
          strncat(info_buffer, String(t).c_str(), 1);
        }

        strcat(info_buffer, "\n  I2C-Adresse (DEC): ");
        strcat(info_buffer, String(i+MCP_I2C_BASE_ADDRESS).c_str());
        //strcat(info_buffer, keyReport.keys);
        strcat(info_buffer, "\n");
        xQueueSend(serial_tx_info_queue, &info_buffer, pdMS_TO_TICKS(1));
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
  char info_buffer[INFO_BUFFER_SIZE] = {'\0'}; 
  char data[5];

  bool acceleration_button_status = 0;
  bool deceleration_button_status = 0;

  uint8_t reading[4];           //every pcf-IC has 4 analog inputs
  uint8_t t, i, j;              //lokal for-loop counter

  TickType_t intervall = pdMS_TO_TICKS(1000); //needed for debug output
  TickType_t last_stackhighwatermark_print = xTaskGetTickCount(); //needed for debug output

  for(;;)
  {
    vTaskDelay(5);

    if (xTaskGetTickCount() - last_stackhighwatermark_print >= intervall) //every second
    {
      last_stackhighwatermark_print = xTaskGetTickCount();
      queue_printf(serial_tx_info_queue, INFO_BUFFER_SIZE, "\n\nan in - Free Stack Space: %d", uxTaskGetStackHighWaterMark(NULL));
    }
    
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
          if(VERBOSE) queue_printf(serial_tx_verbose_queue, VERBOSE_BUFFER_SIZE, "\n[analog input]\n  Wert an Pin %i von PCF-IC mit Adresse %i: %u\n", t, i+72, reading[t]);
          
          if (reading[t] <= ADC_STEP_THRESHOLD) reading[t] = 0;
          else if (reading[t] >= 255 - ADC_STEP_THRESHOLD) reading[t] = 255;

          pcf_list[i].last_reading[t] = reading[t];

          sprintf(data, "%04X", reading[t]);    

          memset(&cmd_buffer[0], '\0', CMD_BUFFER_SIZE); //clear cmd char array

          //create command-string
          strcat(cmd_buffer, "XV");

          if ((i + PCF_I2C_BASE_ADDRESS) == combined_throttle_pin[0] && combined_throttle_pin[1] == t) //&& reading[t] >= ADC_STEP_THRESHOLD && reading[t] <= 25 - ADC_STEP_THRESHOLD) ;
          { 
            if (VERBOSE) queue_printf(serial_tx_verbose_queue, VERBOSE_BUFFER_SIZE, "\n[analog input]\n  Wert an Kombi-Hebel: %u\n", reading[t]);
            //get address in dependency of which of the two buttons (acceleration/deceleration) was pressed
            if (xSemaphoreTake(i2c_mutex, pdMS_TO_TICKS(40)) == pdTRUE) //40ms wait before ignoring/dropping data
            { 
              acceleration_button_status = mcp_list[acceleration_button[0] - MCP_I2C_BASE_ADDRESS].mcp.digitalRead(acceleration_button[1]);
              deceleration_button_status = mcp_list[deceleration_button[0] - MCP_I2C_BASE_ADDRESS].mcp.digitalRead(deceleration_button[1]);
              xSemaphoreGive(i2c_mutex);
            } else {
              if(VERBOSE) queue_printf(serial_tx_verbose_queue, VERBOSE_BUFFER_SIZE, "\n[analog input]\n  Fehler beim Lesen der Buttons.\n");
              continue;
            }

            if (acceleration_button_status) strcat(cmd_buffer, String(acceleration_button[0]).c_str());
            else if (deceleration_button_status) strcat(cmd_buffer, String(deceleration_button[0]).c_str());
            else if (VERBOSE) queue_printf(serial_tx_verbose_queue, VERBOSE_BUFFER_SIZE, "\n[analog input]\n  Kein Button gedrückt.\n");
            else continue;

            strcat(cmd_buffer, pcf_list[i].address[t]);
          }
          else strcat(cmd_buffer, pcf_list[i].address[t]); //just any other analog input
                             
          strcat(cmd_buffer, data);
          strcat(cmd_buffer, "Y");

               
          //if (!VERBOSE && eTaskGetState(Task6) != eRunning) xQueueSend(serial_tx_cmd_queue, &cmd_buffer, pdMS_TO_TICKS(1)); 

          if(eTaskGetState(Task6) == eRunning) //user 
          {
            memset(&info_buffer[0], '\0', CMD_BUFFER_SIZE);              //clear cmd char array

            //create string "Pin: XXX\nI2C-Adresse: 0xXX"
            strcpy(info_buffer, "Pin: ");
            strcat(info_buffer, String(t).c_str());

            strcat(info_buffer, "\n[analog input]\n  I2C-Adresse (DEC): ");
            strcat(info_buffer, String(i+PCF_I2C_BASE_ADDRESS).c_str());
            strcat(info_buffer, cmd_buffer);
            strcat(info_buffer, "\n");

            xQueueSend(serial_tx_info_queue, &info_buffer, pdMS_TO_TICKS(1));
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
    xQueueReceive(tcp_rx_cmd_queue, &payload, portMAX_DELAY); //block this task, if queue is empty
    
    if(VERBOSE) queue_printf(serial_tx_verbose_queue, VERBOSE_BUFFER_SIZE, "\n[output task]\n  TCP Datenpaket empfangen: %s\n", payload.pld);
    int command_index = 2;      //skip the first two bytes
    int value_index = 3;        //skip the first two bytes

    for (i = 0; i < payload.count; i++) 
    {
      if(VERBOSE) queue_printf(serial_tx_verbose_queue, VERBOSE_BUFFER_SIZE, "\n[output task]\n  Befehl: %s\n  Wert: %s", payload.pld[command_index], payload.pld[value_index]);


      float value_cast_to_float = *((float *)&payload.pld[value_index]); //"Single"
      int value_cast_to_int = *((int *)&payload.pld[value_index]);      //"enum" based on value different actions are performed

      int command = atoi(&payload.pld[command_index]);
      

      switch (command) 
      {
        case GESCHWINDIGKEIT: 
            for (t = 0; t < MAX_IC_COUNT; t++)
            {
              if (pcf_list[t].address[0] == GESCHWINDIGKEIT) //pcf ics only have 1 analog output
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
              if (pcf_list[t].address[0] == AFB_SOLL_GESCHWINDIGKEIT) //pcf ics only have 1 analog output
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

                      if (mcp_list[t].address[0] == TUEREN) //mcp ics have 8 digital outputs
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
  char buffer[CMD_BUFFER_SIZE];
  uint8_t t, i;  
  tcp_payload payload;

  TickType_t request_interval = pdMS_TO_TICKS(80); 
  TickType_t last_request = xTaskGetTickCount();       

  TickType_t request_timeout = pdMS_TO_TICKS(5000); // 5 seconds timeout
  TickType_t request_time = xTaskGetTickCount();

  for(;;)
  {
    vTaskDelay(5);

    if (USBSerial.available() > 0) 
    {
      USBSerial.readBytesUntil('\n', buffer, CMD_BUFFER_SIZE);                            
      if (buffer[0] == 'M' || buffer[0] == 'm')  vTaskResume(Task6);
    }


    if (client.connected() && xTaskGetTickCount() - last_request > request_interval) 
    {
      last_request = xTaskGetTickCount();

      while (!client.available() && (xTaskGetTickCount() - request_time) < request_timeout) {
          vTaskDelay(request_interval); 
      }

      if (client.available()) 
      {
          char c[4];
          client.readBytes(c, sizeof(c));  //first 4 bytes indicates payload size
          
          unsigned long payload_len = *(unsigned long *)c; //reinterpret the 4 bytes as an unsigned long
          
          char pld[payload_len];
          for(i = 0; i < payload_len; i ++) { pld[i] = client.read(); }

          int count = (payload_len - 2) / 5; //first two bytes are headers (indicate if a value changed), 5 bytes for each command

          if (count > 2) //did any of the requested values change?
          {
            int size = sizeof(pld); 
            if (size > 3)               //????????why?????????
            {
              payload.pld[size]; //= new char[size];
              memcpy(payload.pld, pld, size);
              payload.count = count;              
              xQueueSend(tcp_rx_cmd_queue, &payload, pdMS_TO_TICKS(1));
            }
          }
      } 
      else 
      {
          if (VERBOSE) queue_printf(serial_tx_verbose_queue, VERBOSE_BUFFER_SIZE, "\n[rx_task]\n  Timeout: Keine Daten vom LOKSIM3D-Server empfangen.\n");
          handshake_and_request();
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

  char buffer[INFO_BUFFER_SIZE]; //only one buffer -> overwrite content after USBSerial.print

  for(;;)
  {
    vTaskDelay(5);

    if (xQueueReceive(keyboard_tx_queue, &keyReport, 1) == pdTRUE)    Keyboard.sendReport(&keyReport);
     
    if (xQueueReceive(serial_tx_verbose_queue, &buffer, 1) == pdTRUE)     USBSerial.print(buffer);
    
    if (eTaskGetState(Task6) == eRunning && xQueueReceive(serial_tx_info_queue, &buffer, 1) == pdTRUE)   USBSerial.print(buffer);
    
  }
}

/*
*configuration menu
*own task to be able to suspend and resume other tasks from within this function
*dont judge me for the code quality, it's just a configuration menu and i am not a professional programmer
*/

void config_task (void * pvParameters)
{
  TickType_t intervall = pdMS_TO_TICKS(1000); //needed for debug output
  TickType_t last_stackhighwatermark_print = xTaskGetTickCount(); //needed for debug output

  for(;;)
  {
    vTaskDelay(5);

    if (xTaskGetTickCount() - last_stackhighwatermark_print >= intervall) {
      last_stackhighwatermark_print = xTaskGetTickCount();
      queue_printf(serial_tx_info_queue, INFO_BUFFER_SIZE, "\n\nconfig- Free Stack Space: %d", uxTaskGetStackHighWaterMark(NULL));
    }

    //suspend tasks to prevent data corruption
    vTaskSuspend(Task1);      //digital input task
    vTaskSuspend(Task2);      //analog input task
    vTaskSuspend(Task4);      //USBSerial rx task     
    vTaskSuspend(Task5);      //USBSerial tx task

    serial_config_menu();  //tasks 1,2&5 can be resumed from within this function

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
    tcp_rx_cmd_queue = xQueueCreate(2, sizeof(tcp_payload)); 
    keyboard_tx_queue = xQueueCreate(2, sizeof(KeyReport));   //stores commands, that are created in both input-tasks and then transmitted by the tx task 
    serial_tx_info_queue = xQueueCreate(3, INFO_BUFFER_SIZE); //stores i2c address and MCP Pin, if run_config_task==true  
    serial_tx_verbose_queue = xQueueCreate(5, VERBOSE_BUFFER_SIZE); //other stuff that can be helpfull for debugging
    
    i2c_mutex = xSemaphoreCreateMutex();

    // To be called before ETH.begin()
    ESP32_W5500_onEvent();
   
    ETH.begin( MISO_GPIO, MOSI_GPIO, SCK_GPIO, CS_GPIO, INT_GPIO, SPI_CLOCK_MHZ, ETH_SPI_HOST, mac );
    
    ESP32_W5500_waitForConnect();

    USBSerial.print(F("\nVerbindung zu LOKSIM3D TCP-Server wird hergestellt...\n"));

    while (!client.connect(host, port, 5000)) { //try to connect to LOKSIM3D server with 5 seconds timeout
      USBSerial.print(F("."));
      vTaskDelay(2000);
    }

    xTaskCreatePinnedToCore(
      rx_task,          /* Task function. */
      "Task4",              /* name of task. */
      2000,                 /* Stack size */
      NULL,                 /* optional parameters*/
      1,                    /* priority */
      &Task4,               /* Task handle to keep track of created task */
      0                     /*pinned to core 0*/
    );
    xTaskCreate(digital_input_task, "Task1", 2000, NULL, 1, &Task1);
    xTaskCreate(analog_input_task, "Task2", 2000, NULL, 1, &Task2);
    xTaskCreate(output_task, "Task3", 2000, NULL, 1, &Task3);
    xTaskCreatePinnedToCore(tx_task, "Task5", 2000, NULL, 1, &Task5, 0);
    xTaskCreate(config_task, "Task6", 8000, NULL, 1, &Task6);

    
    vTaskSuspend(Task6);
    vTaskSuspend(Task4);
    
    //all other tastks are suspended in this function, so that it is possible to call it from the rx_task (task4)
    handshake_and_request(); 
  
    indicate_finished_setup();

    vTaskResume(Task1);
    vTaskResume(Task2);
    vTaskResume(Task3);
    vTaskResume(Task4); //initially suspended, but not resumed in handshake_and_request-function
    vTaskResume(Task5);

    USBSerial.print(F("\nTasks erfolgreich gestartet.\nUm in das Konfigurationsmenü zu gelangen, \"M\" eingeben.\n"));
  
  }
} //namespace lokSim3D_interface
