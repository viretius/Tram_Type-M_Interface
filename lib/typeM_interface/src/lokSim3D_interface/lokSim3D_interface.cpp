/*
cvs files: 
  
  mcp:
  i2c;pin;kanal;io;key;adresse;info ->key for inputs, address for outputs
     
  pcf: 
  i2c;pin;key;address;info

    -> address "tp" for combined throttle 
*/
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
//
//========================================================================================================

void press_and_release_key(KeyReport *keyReport, uint8_t pin) 
{
  //in address there might be something like "ctrl+a" or "ctrl+shift+a" or "a" -> split it and send the keys in the correct order
  char *token = strtok(mcp_list[pin].address[pin], "+");
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

  //clear keyreport and send to queue
  memset(keyReport, 0, sizeof(KeyReport));
  if (eTaskGetState(Task6) != eRunning) xQueueSend(keyboard_tx_queue, &keyReport, pdMS_TO_TICKS(1));
  if (VERBOSE) queue_printf(serial_tx_verbose_queue, VERBOSE_BUFFER_SIZE, "\n  Keyreport: %s", (*keyReport).keys);

}

//========================================================================================================
/*
*helper function for analog_input_task
*maps the value of the potentiometer (throttle, 255 - 0 - 255) in dependency of the two buttons acceleration_indicator and deceleration_indicator to 0 - 255
*/
//unterschiedliche oder dieselben Kanalnummern für Zug- & Bremskraft!

uint8_t calc_throttle_position(uint8_t value) 
{ 

  bool acceleration_button_status = 0;
  bool deceleration_button_status = 0;

  if (xSemaphoreTake(i2c_mutex, pdMS_TO_TICKS(10)) == pdTRUE) 
  { 
    acceleration_button_status = mcp_list[acceleration_button[0] - MCP_I2C_BASE_ADDRESS].mcp.digitalRead(acceleration_button[1]);
    deceleration_button_status = mcp_list[deceleration_button[0] - MCP_I2C_BASE_ADDRESS].mcp.digitalRead(deceleration_button[1]);
    xSemaphoreGive(i2c_mutex);
  }
  else {
    if (VERBOSE) queue_printf(serial_tx_verbose_queue, VERBOSE_BUFFER_SIZE, "\n  Queue-Timeout: Tasterstatus zur Ermittlung der Hebelposition konnte nicht erfasst werden.");
  }

  if (!acceleration_button_status && deceleration_button_status) {
    //return deceleration value
    if(VERBOSE) queue_printf(serial_tx_verbose_queue, VERBOSE_BUFFER_SIZE, "\n  Deceleration: %i", value);
    return ((255 - value) / 2) - 127; 
  }
  else if (acceleration_button_status && !deceleration_button_status) {
    //return acceleration value
    if(VERBOSE) queue_printf(serial_tx_verbose_queue, VERBOSE_BUFFER_SIZE, "\n  Acceleration: %i", value);
    return (value / 2) + 127; 
  }
  
  return 127; //throttle is in 0-position
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
          press_and_release_key(&keyReport, t); //set keyReport according to the pin that changed its state and send to queue
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

void analog_input_task (void * pvParameters)
{
  
  char cmd_buffer[CMD_BUFFER_SIZE] = {'\0'}; 
  char info_buffer[INFO_BUFFER_SIZE] = {'\0'}; 
  char data[5];
  uint8_t reading[4];           //every pcf-IC has 4 analog inputs
  uint8_t t, i, j;                                                 //lokal for-loop counter


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
          if(VERBOSE) queue_printf(serial_tx_verbose_queue, VERBOSE_BUFFER_SIZE, "\n[analog input]\n  Wert an Pin %i von PCF-IC mit Adresse %i: %u\n", t, i+72, reading[t]);
          
          if (reading[t] <= ADC_STEP_THRESHOLD) reading[t] = 0;
          else if (reading[t] >= 255 - ADC_STEP_THRESHOLD) reading[t] = 255;

          pcf_list[i].last_reading[t] = reading[t];

          //if ((i + PCF_I2C_BASE_ADDRESS) == combined_throttle_pin[0] && t == combined_throttle_pin[1])// && reading[t] >= ADC_STEP_THRESHOLD && reading[t] <= 25 - ADC_STEP_THRESHOLD) ;
          { 
            USBSerial.println(calc_throttle_position(reading[t]));
          }
          
          sprintf(data, "%04X", reading[t]);    

          memset(&cmd_buffer[0], '\0', CMD_BUFFER_SIZE);              //clear cmd char array
          //create command-string
          strcat(cmd_buffer, "XV");                          
          strcat(cmd_buffer, pcf_list[i].address[t]);
          strcat(cmd_buffer, data);
          strcat(cmd_buffer, "Y");
                
          if (!VERBOSE && eTaskGetState(Task6) != eRunning) xQueueSend(keyboard_tx_queue, &cmd_buffer, pdMS_TO_TICKS(1)); 

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
    xQueueReceive(tcp_rx_cmd_queue, &cmd_buffer, portMAX_DELAY); //block this task, if queue is empty

    char *buffer_ptr = cmd_buffer + 2;        //store buffer in buffer_ptr, remove first two chars (indicator for cmd-start & indicator for digital/analog data)

    if (cmd_buffer[1] == 'U')                   //digital output
    {
      if(VERBOSE) queue_printf(serial_tx_verbose_queue, VERBOSE_BUFFER_SIZE, "\n[output task]\n  Datentelegramm empfangen: %s", cmd_buffer);
      
      strncpy(address, buffer_ptr, 2);   //copy first two chars to address
      buffer_ptr += 2;                   //remove those first two chars
      strncpy(data, buffer_ptr, 4);      //store remaining four chars in the data char-array

      if(VERBOSE) queue_printf(serial_tx_verbose_queue, VERBOSE_BUFFER_SIZE, "\n  Kanal: %s  Wert: %s", address, data);

      for (i = 0; i < MAX_IC_COUNT; i++) 
      {
        if(!mcp_list[i].enabled) continue;//i2c adress not in use
        for (t = 0; t < 16; t++) 
        {
          if(strncmp(mcp_list[i].address[t], address, 2) != 0) continue;
          
          if (CHECK_BIT(mcp_list[i].portMode, t)) //pin was set as input!
          { 
            if(VERBOSE) queue_printf(serial_tx_verbose_queue, VERBOSE_BUFFER_SIZE, "\n  Pin %i ist als Eingang konfiguriert.\n", t);
            goto ret; //break both for-loops;
          }

          if (xSemaphoreTake(i2c_mutex, pdMS_TO_TICKS(1)) == pdTRUE) 
          { 
            mcp_list[i].mcp.digitalWrite(t, atoi(data)); 
            xSemaphoreGive(i2c_mutex);
          }
          goto ret;  //address was found, no need to look further -> break both for-loops 
        }
      }
      ret: ;
    }

    else if (cmd_buffer[1] =='V')           //analog output
    {
      t = 4; //pcf ICs only have one output. related channel is stored at pcf_list[4]
      if(VERBOSE) queue_printf(serial_tx_verbose_queue, VERBOSE_BUFFER_SIZE, "\n[output task]\n  Datentelegramm empfangen: %s", cmd_buffer);
      
      strncpy(address, buffer_ptr, 2);   //copy first two chars to address+
      buffer_ptr += 2;                   //remove those first two chars
      strncpy(data, buffer_ptr, 4);      //store remaining four chars in the data char-array

      if(VERBOSE) queue_printf(serial_tx_verbose_queue, VERBOSE_BUFFER_SIZE, "\n  Kanal: %s  Wert: %s \n", address, data);

      for (i = 0; i < MAX_IC_COUNT; i++) 
      {
        if(!pcf_list[i].enabled) continue;
        if(strcmp(pcf_list[i].address[t], address) != 0) continue;
        
        sscanf(data, "%04x", &value); //convert hex to dec
        
        if (xSemaphoreTake(i2c_mutex, pdMS_TO_TICKS(1)) == pdTRUE) { //portMAX_DELAY!?
          pcf_list[i].pcf.analogWrite(value);
          xSemaphoreGive(i2c_mutex);
        } 
        break;    
      }
    }  

    else {
      if(VERBOSE) queue_printf(serial_tx_verbose_queue, VERBOSE_BUFFER_SIZE, "\n[output task]\n  Fehlerhaftes Datentelegramm empfangen: %s\n", cmd_buffer);
    }

  } 
}

/*
receive data and add to according queues
*/

void rx_task (void * pvParameters) //also usbserial rx task for config menu
{
  char buffer[CMD_BUFFER_SIZE];
  char verbose_buffer[VERBOSE_BUFFER_SIZE];
  uint8_t t, i;  

  TickType_t request_interval = pdMS_TO_TICKS(100); 
  TickType_t last_request = xTaskGetTickCount();       

  TickType_t request_timeout = pdMS_TO_TICKS(5000); // 5 seconds timeout
  TickType_t request_time = xTaskGetTickCount();

  for(;;)
  {
    vTaskDelay(5);

    if (USBSerial.available() > 0) 
    {
      USBSerial.readBytesUntil('\n', buffer, CMD_BUFFER_SIZE);                            
      if (buffer[0] == 'M' || buffer[0] == 'm') {
        vTaskResume(Task6);
      }
    }


    if (client.connected() && xTaskGetTickCount() - last_request > request_interval) 
    {
      last_request = xTaskGetTickCount();
      client.println("GET /something HTTP/1.1"); //request data
      client.println();

      while (!client.available() && (xTaskGetTickCount() - request_time) < request_timeout) {
          vTaskDelay(pdMS_TO_TICKS(10)); 
      }

      if (client.available() > 0) 
      {
          client.readBytesUntil('\n', buffer, CMD_BUFFER_SIZE); 
          xQueueSend(tcp_rx_cmd_queue, &buffer, pdMS_TO_TICKS(1));
      } 
      else 
      {
          if (VERBOSE) queue_printf(serial_tx_verbose_queue, VERBOSE_BUFFER_SIZE, "\n[rx_task]\n  Timeout: Keine Daten vom LOKSIM3D-Server empfangen.\n");
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
     
    if (VERBOSE && xQueueReceive(serial_tx_verbose_queue, &buffer, 1) == pdTRUE)     USBSerial.print(buffer);
    
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
    tcp_rx_cmd_queue = xQueueCreate(2, CMD_BUFFER_SIZE); 
    keyboard_tx_queue = xQueueCreate(2, sizeof(KeyReport));   //stores commands, that are created in both input-tasks and then transmitted by the tx task 
    serial_tx_info_queue = xQueueCreate(3, INFO_BUFFER_SIZE); //stores i2c address and MCP Pin, if run_config_task==true  
    serial_tx_verbose_queue = xQueueCreate(5, VERBOSE_BUFFER_SIZE); //other stuff that can be helpfull for debugging
    
    i2c_mutex = xSemaphoreCreateMutex();

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
    //suspend tasks before network initialization
    vTaskSuspend(Task1);
    vTaskSuspend(Task2);
    vTaskSuspend(Task3);
    vTaskSuspend(Task5);

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
    vTaskResume(Task5);

    USBSerial.print(F("\nTasks erfolgreich gestartet.\nUm in das Konfigurationsmenü zu gelangen, \"M\" eingeben.\n"));
  
  }
} //namespace lokSim3D_interface
