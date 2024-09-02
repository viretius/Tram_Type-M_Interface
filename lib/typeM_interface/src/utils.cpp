#include <utils.h>  

void USB_Event_Callback(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
  if (event_base == ARDUINO_USB_EVENTS) {
    arduino_usb_event_data_t *data = (arduino_usb_event_data_t *)event_data;
    switch (event_id) {
      case ARDUINO_USB_STARTED_EVENT: Serial.println("USB PLUGGED"); break;
      case ARDUINO_USB_STOPPED_EVENT: Serial.println("USB UNPLUGGED"); break;
      case ARDUINO_USB_SUSPEND_EVENT: Serial.printf("USB SUSPENDED: remote_wakeup_en: %u\n", data->suspend.remote_wakeup_en); break;
      case ARDUINO_USB_RESUME_EVENT:  Serial.println("USB RESUMED"); break;

      default: break;
    }
  } else if (event_base == ARDUINO_USB_CDC_EVENTS) {
    arduino_usb_cdc_event_data_t *data = (arduino_usb_cdc_event_data_t *)event_data;
    switch (event_id) {
      case ARDUINO_USB_CDC_CONNECTED_EVENT:    Serial.println("CDC CONNECTED"); break;
      case ARDUINO_USB_CDC_DISCONNECTED_EVENT: Serial.println("CDC DISCONNECTED"); break;
      case ARDUINO_USB_CDC_LINE_STATE_EVENT:   Serial.printf("CDC LINE STATE: dtr: %u, rts: %u\n", data->line_state.dtr, data->line_state.rts); break;
      case ARDUINO_USB_CDC_LINE_CODING_EVENT:
        Serial.printf(
          "CDC LINE CODING: bit_rate: %lu, data_bits: %u, stop_bits: %u, parity: %u\n", data->line_coding.bit_rate, data->line_coding.data_bits,
          data->line_coding.stop_bits, data->line_coding.parity
        );
        break;
      case ARDUINO_USB_CDC_RX_EVENT:
        Serial.printf("CDC RX [%u]:", data->rx.len);
        {
          uint8_t buf[data->rx.len];
          size_t len = Serial.read(buf, data->rx.len);
          Serial.write(buf, len);
        }
        Serial.println();
        break;
      case ARDUINO_USB_CDC_RX_OVERFLOW_EVENT: Serial.printf("CDC RX Overflow of %d bytes", data->rx_overflow.dropped_bytes); break;

      default: break;
    }
  }
}

void queue_printf(QueueHandle_t queue, int size, const char *format, ...) {
    
    char *buffer = new char[size];
    va_list args;
    va_start(args, format); //format -> last argument before arg-list
    vsnprintf(buffer, size, format, args);
    va_end(args);
    
    if (xQueueSend(queue, &buffer, pdMS_TO_TICKS(100)) == pdFALSE) {
      USBSerial.print(buffer);
      //if (VERBOSE) USBSerial.print("Queue is full. Message dropped.");
    }
    delete buffer;
}

size_t getFilesize(fs::FS &fs, const char* path) 
{
  size_t fsize;
  File file = fs.open(path);
  if (!file || file.isDirectory()) {
    USBSerial.print(F("- Datei konnte nicht geöffnet werden.\n"));
    return 0;
  }
  fsize = file.size(); //get filesize in bytes
  file.close();
  return fsize;
}

void readFile(fs::FS &fs, const char *path, char *buffer, size_t len) 
{
  USBSerial.printf("Datei wird geöffnet: %s\r\n", path);

  File file = fs.open(path);
  if (!file || file.isDirectory()) {
    USBSerial.print(F("- Datei konnte nicht geöffnet werden.\n"));
    return;
  }
  while (file.available()) {
    file.readBytes(buffer, len);
  }
  file.close();
}

void appendFile(fs::FS &fs, const char *path, const char *message) 
{
  //USBSerial.printf("Neue Daten in %s anhängen\r\n", path);

  File file = fs.open(path, FILE_APPEND);
  if (!file) {
    USBSerial.print(F("- Datei konnte nicht geöffnet werden.\n"));
    return;
  }
  if (file.print(message)) {
    //USBSerial.println("- neue Daten erfolgreich eingefügt.");
  } else {
    USBSerial.println("- neue Daten konnten nicht angefügt werden.");
  }
  file.close();
}

void clearFile(fs::FS &fs, const char *path) 
{
    File file = fs.open(path, FILE_WRITE);
    if (!file) {
        USBSerial.println("Datei konnte nicht geöffnet werden.");
        return;
    }
    file.close();  // Truncate the file to zero length
}

//=======================================================================================================================================
//print io configuration in binary format (only used in config_task!, because its not using a queue but printing directly with USBSerial)
//=======================================================================================================================================

void printBinary(uint16_t value) 
{
    
    for (int t = 0; t < 16; t++) { //print portMode in binary format
      USBSerial.printf("%i", (CHECK_BIT(value, t)) ? 1 : 0 ); //at USBSerial monitor, bit 0 represents pin 0 and bit 15 pin 15
    }
    USBSerial.println();
}
//=======================================================================================================================================


//==========================================================================================================================
//resume input tasks and print out infos for the user
//==========================================================================================================================

void opt_1() 
{
    run_config_task = 1;  //set flag to return to menu after this function returns
    USBSerial.print(F("trainsim config"));
    USBSerial.print(F("Sie können jetzt einen beliebigen Taster/Schalter/Hebel betätigen."));
    USBSerial.print(F("\nUm wieder zurück zum Menü zu gelangen, geben Sie ein \"C\" ein.\n"));    

    vTaskResume(Task1);
    vTaskResume(Task2);
    vTaskResume(Task5);

    while(USBSerial.read() != 'C'){vTaskDelay(1);}
    
    vTaskSuspend(Task1);
    vTaskSuspend(Task2);
    vTaskSuspend(Task5);

}

//==========================================================================================================================
//print current config in CSV style and go back to menu (not ready yet)
//==========================================================================================================================
void opt_2() 
{
    int i, t;
    run_config_task = 1;
  
    USBSerial.print(F("Die Ausgabe erfolgt sortiert nach der I2C-Adresse (DEC nicht HEX!).\nEs werden erst alle MCP-konfigurationen gelistet gefolgt von denen der PCF-ICs.\n"));

    USBSerial.print(F("\nmcp-Konfiguration:\n\ni2c;pin;kanal/adresse/tastenkombination;io;info"));
    
     for (i = 0; i < MAX_IC_COUNT; i++) 
     {
      if(!mcp_list[i].enabled) continue;//i2c adress not in use
      for (t = 0; t < 16; t++)  
      { 
        USBSerial.printf("\n%i;", mcp_list[i].i2c);
        USBSerial.printf("%i;", t);
        USBSerial.printf("%s;", mcp_list[i].address[t]);
        USBSerial.printf("%i", (mcp_list[i].portMode & (1 << (t)))? 1:0); //check if bit is set a position t
        USBSerial.printf("%s", mcp_list[i].info[t]);
            
      }
    }

    USBSerial.print(F("\n\npcf-Konfiguration:\n\ni2c;pin;kanal/adresse/tastenkombination;info"));

    for (i = 0; i < MAX_IC_COUNT; i++) 
    {
      if(!pcf_list[i].enabled) continue;//i2c adress not in use
      for (t = 0; t < 5; t++) 
      {
        USBSerial.printf("\n%i;", pcf_list[i].i2c);
        USBSerial.printf("%i;", t);
        USBSerial.printf("%s;", pcf_list[i].address[t]);
        USBSerial.printf("%s", pcf_list[i].info[t]);
      }
    }
    USBSerial.print(F("\n\nMenü wird erneut aufgerufen..."));
    vTaskDelay(pdMS_TO_TICKS(1000));
}

//==========================================================================================================================
//Change portConfig or single pin config of a specific MCP IC  (not ready yet) only working for simMetro
//==========================================================================================================================

void opt_3() 
{
  char *buffer = new char[17]{'\0'};
  int i, t;
  int i2c_adr, pin;
  char info[250];        //info for user, if input didnt fit to request, static array, because there is enough memory, but memory allocation costs cpu cycles
  int option;

  USBSerial.print(F("\nSie können jederzeit zum Menü zurückgelangen. Geben Sie dazu ein \"C\" ein."));   

  USBSerial.printf("\nGeben Sie die I2C-Addresse (%i bis %i) des MCP-ICs ein, dessen Port-Konfiguration Sie ändern möchten.", MCP_I2C_BASE_ADDRESS, MCP_I2C_END_ADDRESS);  
  strcpy(info, "\nDiese Adresse steht nicht zur verfügung.\nGeben Sie eine passende Adresse ein, oder kehren Sie mit \"C\" zurück zum Menü.\n");
  if (get_integer_with_range_check(&i2c_adr, MCP_I2C_BASE_ADDRESS, MCP_I2C_END_ADDRESS, info)) return; 

  USBSerial.print(F("\n\nAktuelle Portkonfiguration:\n")); 
  for (i = 0; i < MAX_IC_COUNT; i++) {
    if (mcp_list[i].i2c == i2c_adr) break;     
  }
  printBinary(mcp_list[i].portMode);

  memset(&buffer[0], '\0', 17); //clear buffer
  USBSerial.print(F("\n\nUm die Portkonfiguration dieses ICs zu ändern, geben Sie eine \"1\" ein. Um nur einen einzelnen Pin anzupassen, geben Sie eine \"2\" ein."));
  strcpy(info, "\nDiese Option steht nicht zur verfügung.\nGeben Sie eine \"1\" oder eine \"2\" ein, oder kehren Sie mit \"C\" zurück zum Menü.\n");
  if (get_integer_with_range_check(&option, 1, 2, info)) return;

  if (option == 1) 
  {
    USBSerial.print(F("\nGeben Sie die neue Konfiguration jetzt im selben Format ein. Das erste Bit eines Bytes entspricht dabei Pin 7 des entsprechenden Ports. Bei einer 1 wird der entsprechende Pin als Eingang Konfiguriert."));
    USBSerial.print(F("\nPort A ist dem ersten Byte und Port B dem zweiten Byte zugeordnet. Pins A07 und B07 (Bit 0 und Bit 8) sollten nur als Ausgänge konfiguriert werden!"));
    USBSerial.print(F("\nVorsicht! Die Eingabe wird ungeprüft gespeichert!"));
    memset(&buffer[0], '\0', 16);
    while(!USBSerial.available()) {vTaskDelay(1);}
    if (USBSerial.peek() == 'C') return;
    else 
    { 
      USBSerial.readBytesUntil('\n', buffer, 16);
      for (t = 0; t < 16; t++) 
      {
       buffer[t] = (buffer[t] == '1') ? '1':'0'; //replace all characters that are not '1' with '0'
       buffer[t] == '1' ? bitSet(mcp_list[i].portMode, t) : bitClear(mcp_list[i].portMode, t); //set or clear bit
      }
    }    
    USBSerial.print(F("\nPortkonfiguration geändert zu: \n"));
    printBinary(mcp_list[i].portMode);
  }

  else if (option == 2) 
  {
    USBSerial.print(F("\nGeben sie eine Nummer von 0 bis 15 für den Pin ein, den Sie ändern möchten.\n"));  
    memset(&buffer[0], '\0', 17); //clear buffer
    strcpy(info, "\nDiesen Pin gibt es nicht\nGeben sie eine Nummer von 0 bis 15 für den Pin ein, den Sie ändern möchten oder kehren Sie mit \"C\" zurück zum Menü.\n");
    if (get_integer_with_range_check(&pin, 0, 15, info)) return;
    USBSerial.printf("\nPin %i wird umgestellt von %i", pin, (CHECK_BIT(mcp_list[i].portMode, pin) ? 1:0));
    
    bitWrite(mcp_list[i].portMode, pin, !CHECK_BIT(mcp_list[i].portMode, pin)); //toggle bit

  }
 
  USBSerial.printf(" auf %i\n", (CHECK_BIT(mcp_list[i].portMode, pin) ? 1:0));

  USBSerial.printf("mcp_list[i].portMode: ");
  printBinary(mcp_list[i].portMode);

  if (xSemaphoreTake(i2c_mutex, portMAX_DELAY) == pdTRUE) 
  { 

    USBSerial.printf("Setting portMode for Port A: ");
    printBinary(mcp_list[i].portMode & 0x00FF);      
    mcp_list[i].mcp.portMode(MCP23017Port::A, mcp_list[i].portMode & 0x00FF); //input_pullups enabled by default, portA: LSB
    mcp_list[i].mcp.writeRegister(MCP23017Register::GPIO_A, 0x00);
    USBSerial.println("portA set");

    //vTaskDelay(1);

    USBSerial.printf("\nSetting portMode for Port B: ");
    printBinary((mcp_list[i].portMode >> 8) & 0x00FF);
    mcp_list[i].mcp.portMode(MCP23017Port::B, (mcp_list[i].portMode >> 8) & 0x00FF); //portB: MSB
    mcp_list[i].mcp.writeRegister(MCP23017Register::GPIO_B, 0x00);  //Reset ports
    USBSerial.println("portB set");
    
    vTaskDelay(pdMS_TO_TICKS(1));
    mcp_list[i].last_reading = mcp_list[i].mcp.read();
    
    xSemaphoreGive(i2c_mutex);
  }

  while (USBSerial.available()) {USBSerial.read();}
  USBSerial.print(F("\nMenü erneut aufrufen? (j/n)"));
  USBSerial.print(F("\n(Eine andere Eingabe als j wird als Nein interpretiert)\n"));

  while(!USBSerial.available()){vTaskDelay(1);}
  memset(&buffer[0], '\0', 3); 
  USBSerial.readBytesUntil('\n', buffer, 1); 
  buffer[1] = '\0';
  USBSerial.printf("\n%s", buffer);
  if (strcmp(buffer, "j") == 0) run_config_task = 1;
  delete[] buffer;
  
}




//==========================================================================================================================
//change number of active ICs (not ready yet)
//==========================================================================================================================
void opt_5() 
{ 
  char info[250];        //info for user, if input didnt fit to request, static array, because there is enough memory, but memory allocation costs cpu cycles
  int mcp_count = 0;
  int pcf_count = 0;

  uint8_t i, t, count;  //counter for number of free i2c addresses
  int new_mcp_count = 0, new_pcf_count = 0; //user input
  int option;  //M -> change num of mcp ics, P-> change num of pcf ics
  int enable_i2c_adr; //user enteres multiple addresses he wants to add;

  for (i = 0; i < MAX_IC_COUNT; i++) 
  {
    if (mcp_list[i + MCP_I2C_BASE_ADDRESS].enabled) mcp_count++;
    if (pcf_list[i + PCF_I2C_BASE_ADDRESS].enabled) pcf_count++;
  }
  USBSerial.print(F("\nSie können jederzeit zum Menü zurückgelangen. Geben Sie dazu ein \"C\" ein.")); 
  USBSerial.printf("\nAktuell sind %u MCP Port-Expander und %u PCF-ADCs in Verwendung.", mcp_count, pcf_count);
  USBSerial.print(F("\nUm die Anzahl der MCP-ICs zu ändern, geben Sie eine \"1\" ein."));
  USBSerial.print(F("\nUm die Anzahl der PCF-ICs zu ändern, geben Sie eine \"2\" ein."));
  USBSerial.print(F("\nAchtung! Bei neuen ICs werden alle Kanäle mit \"-1\" initialisiert.\nWird die Anzahl an ICs verringert, werden die Kanalnummern des entsprechenden ICs ebenfalls auf \"-1\" zurückgesetzt."));
  
  strcpy(info,  "\nDiese Option steht nicht zur verfügung.\nGeben Sie eine \"1\" oder eine \"2\" ein, oder kehren Sie mit \"C\" zurück zum Menü.\n");
  if (get_integer_with_range_check(&option, 1, 2, info)) return;
  while (USBSerial.available() > 0) {USBSerial.read();}
  
  if (option == 1)
  {
    USBSerial.print(F("\nGeben Sie die gewünschte Zahl an Port Expandern an. (max. 8)\n"));
    strcpy(info, "\nDiese Zahl an Portexpandern ist nicht umsetzbar. Geben Sie eine Zahl von 0 bis 8 ein, oder kehren Sie mit \"C\" zurück zum Menü.");
    if (get_integer_with_range_check(&new_mcp_count, 0, 8, info)) return;
    
    if(new_mcp_count > mcp_count) //user wants to add mcp ICs
    {
      USBSerial.print(F("Freie I2C Adressen: "));
      for(i = 0; i < 8; i++) 
      {
        if (mcp_list[i].enabled) continue;
        USBSerial.printf("%x (%i), ", i+32, i);
        count++;
      }

      strcpy(info, "\nDiese Addresse kann nicht vergeben werden. Geben Sie eine Zahl von 32 bis 39 ein, oder kehren Sie mit \"C\" zurück zum Menü.");
      count = (new_mcp_count - mcp_count);
      t = 0;
      while (t < count) //give free addresses to "new" mcp ICs
      {
        USBSerial.printf("\nGeben Sie eine der freien Adressen ein, die sie für IC-Nr. %i vergeben möchten, oder kehren Sie mit \"C\" zurück zum Menü.\n", t+1);
        if (get_integer_with_range_check(&enable_i2c_adr, MCP_I2C_BASE_ADDRESS, MCP_I2C_END_ADDRESS, info)) return;
        for (i = 0; i < MAX_IC_COUNT; i++) //does userinput match a free address?
        {
          if (mcp_list[i].enabled || mcp_list[i].i2c != enable_i2c_adr) continue; 
          
          mcp_list[i].enabled = true;
          if (xSemaphoreTake(i2c_mutex, portMAX_DELAY) == pdTRUE) 
          {        
            if (!mcp_list[i].mcp.init(mcp_list[i].i2c)) {
              USBSerial.printf("\n\nMCP23017 mit der Adresse %x konnte nicht initialisiert werden.", mcp_list[i].i2c);
              xSemaphoreGive(i2c_mutex);
              goto failed_to_init;
            }
          
            mcp_list[i].mcp.portMode(MCP23017Port::A, mcp_list[i].portMode & 0x00FF); //input_pullups enabled by default, portA: LSB
            USBSerial.println("portA set");
            mcp_list[i].mcp.portMode(MCP23017Port::B, (mcp_list[i].portMode >> 8) & 0x00FF); //portB: MSB
            USBSerial.println("portB set");

            //vTaskDelay(1);
            mcp_list[i].mcp.writeRegister(MCP23017Register::GPIO_A, 0x00);
            mcp_list[i].mcp.writeRegister(MCP23017Register::GPIO_B, 0x00);  //Reset ports
  
            mcp_list[i].last_reading = mcp_list[i].mcp.read();
              
            xSemaphoreGive(i2c_mutex);
          }
          t++; 
          break;
        }
        if (i = MAX_IC_COUNT) USBSerial.print(F("\nDiese Adresse ist schon aktiviert."));
      }
      USBSerial.print(F("\nMCP-ICs sind jetzt eingebunden."));
    } 
    else if(new_mcp_count < mcp_count) //user wants to "deactivate" mcp ICs
    {
      for (i = new_mcp_count; i < mcp_count; i++) 
      {
        strcpy(info, "\nDiese Addresse gibt es nicht. Geben Sie eine Zahl von 32 bis 39 ein, oder kehren Sie mit \"C\" zurück zum Menü.");
        count = (mcp_count - new_mcp_count);
        t = 0;
        while (t < count) //give free addresses to "new" mcp ICs
        {
          USBSerial.print(F("\nGeben Sie die Adresse ein, die Sie deaktivieren möchten, oder kehren Sie mit \"C\" zurück zum Menü.\nDie Konfiguration bleiben zur Laufzeit erhalten.\nKonfigurationen deaktivierter ICs werden nicht im Flashspeicher gesichert."));
          if (get_integer_with_range_check(&enable_i2c_adr, MCP_I2C_BASE_ADDRESS, MCP_I2C_END_ADDRESS, info)) return;
          for (i = 0; i < MAX_IC_COUNT; i++) //does userinput match a free address?
          {
            if (!mcp_list[i].enabled || mcp_list[i].i2c != enable_i2c_adr) continue;
            
            if (xSemaphoreTake(i2c_mutex, portMAX_DELAY) == pdTRUE) 
            {
              mcp_list[i].enabled = false;
              mcp_list[i].mcp.writeRegister(MCP23017Register::GPIO_A, 0x00);
              mcp_list[i].mcp.writeRegister(MCP23017Register::GPIO_B, 0x00);  //Reset ports
                
              xSemaphoreGive(i2c_mutex);
            }
            t++;
            break;
          }
          if (i = MAX_IC_COUNT) USBSerial.print(F("\nDiese Adresse ist schon deaktiviert."));
        }
      }
      USBSerial.print(F("\nZahl der aktiven MCP-ICs wurde reduziert."));
    }
  }
  else if (option == 2) 
  {
    USBSerial.print(F("\nGeben Sie die gewünschte Zahl an PCF-ICs an. (max. 8)\n"));
    strcpy(info, "\nDiese Zahl an PCF-ICs ist nicht umsetzbar. Geben Sie eine Zahl von 0 bis 8 ein, oder kehren Sie mit \"C\" zurück zum Menü.");
    if (get_integer_with_range_check(&new_pcf_count, 0, 8, info)) return;

    if (new_pcf_count > pcf_count) 
    {
      USBSerial.print(F("Freie I2C Adressen: "));
      for(i = 0; i < MAX_IC_COUNT; i++) 
      {
        if (pcf_list[i].enabled) continue;
        USBSerial.printf("%i (%02x), ", i + PCF_I2C_BASE_ADDRESS, i + PCF_I2C_BASE_ADDRESS);
        count++;
      }
      
      strcpy(info, "\nDiese Addresse kann nicht vergeben werden. Geben Sie eine Zahl von 72 bis 79 ein, oder kehren Sie mit \"C\" zurück zum Menü.");
      count = (new_pcf_count - pcf_count);
      t = 0;
      while (t < count) //give free addresses to "new" mcp ICs
      {
        USBSerial.printf("\nGeben Sie eine der freien Adressen ein, die sie für IC-Nr. %i vergeben möchten, oder kehren Sie mit \"C\" zurück zum Menü.\n", t+1);
        if (get_integer_with_range_check(&enable_i2c_adr, PCF_I2C_BASE_ADDRESS, PCF_I2C_END_ADDRESS, info)) return;
        for (i = 0; i < MAX_IC_COUNT; i++) //does userinput match a free address?
        {
          if (pcf_list[i].enabled || pcf_list[i].i2c != enable_i2c_adr) continue; 
          
          pcf_list[i].enabled = true;
          if (xSemaphoreTake(i2c_mutex, portMAX_DELAY) == pdTRUE) 
          {
            if (!pcf_list[i].pcf.begin(pcf_list[i].i2c)) 
            {                 
              USBSerial.printf("\n\nPCF8591 mit der Adresse %x konnte nicht initialisiert werden.", pcf_list[i].i2c);
              xSemaphoreGive(i2c_mutex);
              goto failed_to_init;
            }  
            pcf_list[i].pcf.enableDAC(true);
            pcf_list[i].pcf.analogWrite(0);                                                            
              
            xSemaphoreGive(i2c_mutex);
          }
          t++; 
          break;        
        }
        if (i = MAX_IC_COUNT) USBSerial.print(F("\nDiese Adresse ist schon aktiviert."));
      }
      USBSerial.print(F("\nZahl der PCF-ICs wurde reduziert."));
      USBSerial.print(F("\nDie für diese Laufzeit gespeicherten Kanäle belieben weiterhin bestehen, bis das Gerät abgeschaltet wird oder die Änderungen dauerhaft gesichert werden.\n"));
      
    }
    else if(new_pcf_count < pcf_count) //user wants to "delete" pcf ICs
    {
      for (i = new_pcf_count; i < pcf_count; i++) 
      {
        strcpy(info, "\nDiese Addresse gibt es nicht. Geben Sie eine Zahl von 72 bis 79 ein, oder kehren Sie mit \"C\" zurück zum Menü.");
        count = (pcf_count - new_pcf_count);
        t = 0;
        while (t < count) //give free addresses to "new" mcp ICs
        {
          USBSerial.print(F("\nGeben Sie die Adresse ein, die Sie deaktivieren möchten, oder kehren Sie mit \"C\" zurück zum Menü.\nKanalnummern und Portkonfiguration werden nicht zurückgesetzt.\nDeaktivierte ICs werden nicht im Flashspeicher gesichert."));
          if (get_integer_with_range_check(&enable_i2c_adr, PCF_I2C_BASE_ADDRESS, PCF_I2C_END_ADDRESS, info)) return;
          for (i = 0; i < MAX_IC_COUNT; i++) //does userinput match a free address?
          {
            if (!pcf_list[i].enabled || pcf_list[i].i2c != enable_i2c_adr) continue;
            if (xSemaphoreTake(i2c_mutex, portMAX_DELAY) == pdTRUE) 
            {
              pcf_list[i].enabled = false;
              pcf_list[i].pcf.analogWrite(0);
              pcf_list[i].pcf.enableDAC(false);
            
              xSemaphoreGive(i2c_mutex);
              break; //break for loop
            }
            t++;
          }
          if (i = MAX_IC_COUNT) USBSerial.print(F("\nDiese Adresse ist schon deaktiviert."));
        }
      }
      USBSerial.print(F("\nZahl der aktiven PCF-ICs wurde reduziert."));
    }
  }
  failed_to_init:
  
  while (USBSerial.available()) {USBSerial.read();}
  USBSerial.print(F("\nMenü erneut aufrufen? (j/n)"));
  USBSerial.print(F("\n(Eine andere Eingabe als j wird als Nein interpretiert)\n"));
  while(!USBSerial.available()){vTaskDelay(1);}

  char *buffer = new char[3]{'\0'}; //16bits + null-terminator
  memset(&buffer[0], '\0', 3); 
  USBSerial.readBytesUntil('\n', buffer, 1); 
  USBSerial.printf("\n%s", buffer);
  if (strcmp(buffer, "j") == 0) run_config_task = 1;
  delete[] buffer;
}

//=======================================================================================================================================
//option 6 for config menu in config_file_utils - toggle verbose output 
//=======================================================================================================================================
void toggle_verbose() 
{
  if(VERBOSE) 
    {
        VERBOSE = 0;
        USBSerial.print(F("Debugging-Ausgabe ausgeschaltet."));
    } 
    else {
        VERBOSE = 1;
        USBSerial.print(F("Debugging-Ausgabe eingeschaltet."));
    }
    run_config_task = 1;
}

//==========================================================================================================================
//option 7 for config menu in config_file_utils - store current config in flash (changes by user are temporary)
//==========================================================================================================================

void commit_config_to_fs()  
{ 

  char *buffer = new char[2]{'\0'};
  USBSerial.print(F("\"S\" - Um die Änderungen im Flash zu sichern und die alte Konfiguration zu überschreiben, geben Sie ein \"S\" ein."));
  USBSerial.print(F("\nKehren Sie mit \"C\" zurück zum Menü."));   
  
  while(USBSerial.available()<1){vTaskDelay(1);}
  USBSerial.readBytesUntil('\n', buffer, 2);
  USBSerial.println(buffer);

  if (buffer[0] == 'C') return;

  else if (buffer[0]  == 'S')
  {
    int i, t;
    int mcp_count = 0;
    int pcf_count = 0;
    char info[250];        //info for user, if input didnt fit to request, static array, because there is enough memory, but memory allocation costs cpu cycles
    char *buffer;      //buffer for user-input
    const char *mcp_csvHeader = "i2c;pin;kanal;io;key;addresse;info\n";
    const char *pcf_csvHeader = "i2c;pin;kanal;key;addresse;info\n";

    for (i = 0; i < MAX_IC_COUNT; i++) 
    {
      if (mcp_list[i + MCP_I2C_BASE_ADDRESS].enabled) mcp_count++;
      if (pcf_list[i + PCF_I2C_BASE_ADDRESS].enabled) pcf_count++;
    }

    char *mcp_config = new char[getFilesize(LittleFS, "/mcp.txt")];    //17 + mcp_count * 16 * 10 + 1]; //23: length of header, ic_count * 16 pins * 8chars/pin (i2cadr(2) ";" pin "; &address ";" io "\n") + null-terminator
    char *pcf_config = new char[getFilesize(LittleFS, "/pcf.txt")];            //14 + pcf_count * 5 * 9 + 1]; 

    USBSerial.print(F("\nKonfigurationen werden überschrieben. Bitte warten..."));

    clearFile(LittleFS, "/mcp.txt");
    clearFile(LittleFS, "/pcf.txt");
    
    USBSerial.print(F("\nAktualisiere mcp.txt..."));
    appendFile(LittleFS, "/mcp.txt", mcp_csvHeader);

    for (i = 0; i < MAX_IC_COUNT; i++) {
      if (mcp_list[i].enabled) 
      { 
        for (t = 0; t < 16; t++) 
        {
          //if user enables a new ic that has never been active before, every address will be set to == "-1"
          //user might only change some of them and not every single one

          if (strcmp(mcp_list[i].address[t], "-1") != 0) //strcmp returns 0 if strings are identical
          {
            //1. determin length of row in csv file in bytes
            //2. create buffer with this length
            //3. fill buffer with data from mcp_list
            //4. append buffer to file
            int len = snprintf(NULL, 0, "%u;%u;%s;%u;%s;%s;%s\n", sizeof(uint8_t), sizeof(uint8_t), mcp_list[i].address[t], sizeof(uint8_t), mcp_list[i].address[t], mcp_list[i].address[t],  mcp_list[i].info[t]) + 1;
            buffer = new char[len];
            snprintf(buffer, len, "%u;%u;%s;%u;%s;%s;%s\n", mcp_list[i].i2c, t, mcp_list[i].address[t], (CHECK_BIT(mcp_list[i].portMode, t)) ? 1:0, mcp_list[i].address[t], mcp_list[i].address[t], mcp_list[i].info[t]);
            appendFile(LittleFS, "/mcp.txt", buffer);
            //USBSerial.printf("%s\n", buffer); //debugging purposes
          }
        }
      }
    }
    USBSerial.print(F("\nAktualisiere pcf.txt..."));
    appendFile(LittleFS, "/pcf.txt", pcf_csvHeader);

    for (i = 0; i < MAX_IC_COUNT; i++) {
      if (pcf_list[i].enabled) 
      { 
        for (t = 0; t < 5; t++) 
        {
          if (strcmp(pcf_list[i].address[t], "-1") != 0) 
          {
            int len = snprintf(NULL, 0, "%u;%u;%s;%s;%s;%s\n", sizeof(uint8_t), sizeof(uint8_t), pcf_list[i].address[t], pcf_list[i].address[t], pcf_list[i].address[t], pcf_list[i].info[t]) + 1; //+1 for null-terminator
            buffer = new char[len];
            snprintf(buffer, len, "%u;%u;%s;%s;%s;%s\n", pcf_list[i].i2c, t, pcf_list[i].address[t], pcf_list[i].address[t], pcf_list[i].address[t], pcf_list[i].info[t]);
            appendFile(LittleFS, "/pcf.txt", buffer);
            //USBSerial.printf("%s\n", buffer); 
          }
        }
      }
    }

    USBSerial.print(F("\nKonfigurationen erfolgreich gespeichert."));
    
    delete[] mcp_config;
    delete[] pcf_config;
  }
  delete[] buffer;

}
//==========================================================================================================================
//let user enter a number with specific bounds and max 16 digits
//==========================================================================================================================

//==========================================================================================================================
//choose which interface should be loaded after reboot
//==========================================================================================================================
void opt_10() 
{
  char info[250] = {'\0'};
  int option = 0;
   
  USBSerial.print(F("Kehren Sie mit \"C\" zurück zum Menü.\n"));   
  USBSerial.print(F("Welche Schnittstelle soll nach dem nächsten Neustart geladen werden?\n\n1 -> SimMetro\n2 -> LokSim3D\n"));

  strcpy(info,  "\nDiese Option steht nicht zur verfügung.\nGeben Sie eine \"1\" oder eine \"2\" ein, oder kehren Sie mit \"C\" zurück zum Menü.\n");
  if (get_integer_with_range_check(&option, 1, 2, info)) return;
  
  preferences.begin("APP", false, "nvs");

  if (option == 1) 
  {
    preferences.putUInt("app_setting", option);
    
    USBSerial.print(F("\nSimMetro Schnittstelle wird nach dem nächsten Neustart geladen."));
  } 
  else if (option == 2) 
  {
    preferences.putUInt("app_setting", option);
    USBSerial.print(F("\nLokSim3D Schnittstelle wird nach dem nächsten Neustart geladen."));
  } 
  preferences.end();

  if (option) run_config_task = 1; //return to menu, where user can reset the device with option 9
}


//==========================================================================================================================
//
//==========================================================================================================================

bool get_integer_with_range_check (int *var, uint lower_bound, uint upper_bound, char *info) //returns true (exit to menu) if user entered "C"
{  
  char* buffer = new char[16 + 1]; // Puffer für Eingabe + 1 für '\0'
  while (USBSerial.available()) USBSerial.read();
  while(!USBSerial.available()) {vTaskDelay(1);} //wait for user-input
  if (USBSerial.peek() == 'C' || USBSerial.peek() == 'c' ) return true;

  int bytesRead = 0;
  while (USBSerial.available()) 
  {
    bytesRead += USBSerial.readBytesUntil('\n', buffer + bytesRead, 16 - bytesRead);
    if (bytesRead > 0 || buffer[0] == '\n') break;
    vTaskDelay(1); 
  }

  buffer[bytesRead] = '\0'; // String korrekt terminieren
  USBSerial.printf("\n%s", buffer);

  int error = sscanf(buffer, "%u", var);

  while (error != 1 || *var < lower_bound || *var > upper_bound) 
  {
    USBSerial.printf("\nIhre Eingabe: %s", buffer);
    memset(buffer, '\0', 16 + 1); // Puffer leeren
    USBSerial.printf("\n%s\n", info);

    while (USBSerial.available()) USBSerial.read();

    while (!USBSerial.available()) vTaskDelay(1); //wait for userinput
        
    if (USBSerial.peek() == 'C') return true;
        
    bytesRead = 0;
    int bytesRead = 0;
    while (USBSerial.available()) 
    {
      bytesRead += USBSerial.readBytesUntil('\n', buffer + bytesRead, 16 - bytesRead);
      if (bytesRead > 0 || buffer[0] == '\n') break;
      vTaskDelay(1); 
    }

    buffer[bytesRead] = '\0'; // String korrekt terminieren
    USBSerial.printf("\n%s", buffer);
    error = sscanf(buffer, "%u", var);

    if (error != 1) USBSerial.println("Bitte eine Nummer eingeben."); // Ungültige Eingabe
      
  }

  while (USBSerial.available()) USBSerial.read(); //clear USBSerial hw buffer

  return false;
}

//======================================================================
//optic feedback for user, to let him know, that the setup is finished 
//======================================================================

void indicate_finished_setup() 
{
  int i, j, t;
  
  //turn on all outputs
  for (i = 0; i < MAX_IC_COUNT; i++) 
  {
    if (!mcp_list[i].enabled) continue;
    for (t = 0; t < 16; t++) 
    {
      if (CHECK_BIT(mcp_list[i].portMode, t)) continue; //only set ouputs 
      mcp_list[i].mcp.digitalWrite(t, 1);
    }
  }

  for(i = 0; i <= 255; i++) 
  {
    for (t = 0; t < MAX_IC_COUNT; t++) 
    {
      if (!pcf_list[t].enabled) continue;
      pcf_list[t].pcf.analogWrite(i);
      delay(3);
    }
  }
  delay(100);

  //turn off outputs
  for (i = 0; i < MAX_IC_COUNT; i++) 
  {
    if (!mcp_list[i].enabled) continue;
    for (t = 0; t < 16; t++) 
    {
      if (CHECK_BIT(mcp_list[i].portMode, t)) continue; //only set ouputs 
      mcp_list[i].mcp.digitalWrite(t, 0);
    }
  }
  
  for(i = 255; i >= 0; i--) 
  {
    for (t = 0; t < MAX_IC_COUNT; t++) 
    {
      if (!pcf_list[t].enabled) continue;
      pcf_list[t].pcf.analogWrite(i);
    }
  }  

  // Ensure the analog output is set to 0
  for (t = 0; t < MAX_IC_COUNT; t++) 
  {
    if (!pcf_list[t].enabled) continue;
    pcf_list[t].pcf.analogWrite(0);
  }

}




//======================================================================
//partition find functions
//======================================================================

// Get the string name of type enum values
static const char* get_type_str(esp_partition_type_t type)
{
    switch(type) {
        case ESP_PARTITION_TYPE_APP:
            return "ESP_PARTITION_TYPE_APP";
        case ESP_PARTITION_TYPE_DATA:
            return "ESP_PARTITION_TYPE_DATA";
        default:
            return "UNKNOWN_PARTITION_TYPE"; // type not used in this example
    }
}

// Get the string name of subtype enum values
static const char* get_subtype_str(esp_partition_subtype_t subtype)
{
    switch(subtype) {
        case ESP_PARTITION_SUBTYPE_DATA_NVS:
            return "ESP_PARTITION_SUBTYPE_DATA_NVS";
        case ESP_PARTITION_SUBTYPE_DATA_PHY:
            return "ESP_PARTITION_SUBTYPE_DATA_PHY";
        case ESP_PARTITION_SUBTYPE_APP_FACTORY:
            return "ESP_PARTITION_SUBTYPE_APP_FACTORY";
        case ESP_PARTITION_SUBTYPE_DATA_FAT:
            return "ESP_PARTITION_SUBTYPE_DATA_FAT";
        default:
            return "UNKNOWN_PARTITION_SUBTYPE"; // subtype not used in this example
    }
}

void find_and_print_partitions() {

  USBSerial.println("\n\n----------------Find partitions---------------\n");
      
  esp_partition_iterator_t it;
  it = esp_partition_find(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, NULL);

  // Loop through all matching partitions, in this case, all with the type 'data' until partition with desired 
  // label is found. Verify if its the same instance as the one found before.
  for (; it != NULL; it = esp_partition_next(it)) {
      const esp_partition_t *part = esp_partition_get(it);
      USBSerial.printf("found partition '%s' at offset 0x%x with size 0x%x\n", part->label, part->address, part->size);
  }
  esp_partition_iterator_release(it);

  it = esp_partition_find(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, NULL);

  // Loop through all matching partitions, in this case, all with the type 'data' until partition with desired 
  // label is found. Verify if its the same instance as the one found before.
    for (; it != NULL; it = esp_partition_next(it)) {
      const esp_partition_t *part = esp_partition_get(it);
      USBSerial.printf("found partition '%s' at offset 0x%x with size 0x%x\n", part->label, part->address, part->size);
  }

  // Release the partition iterator to release memory allocated for it
  esp_partition_iterator_release(it);
  USBSerial.println("\n----------------End of partitions---------------\n\n\n\n");
}
