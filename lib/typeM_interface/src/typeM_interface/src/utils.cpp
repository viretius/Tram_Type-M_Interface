#include <utils.h>  

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
//option 7 for config menu in config_file_utils - store current config in flash (changes by user are temporary) - not working yet!!!!!!!
//==========================================================================================================================

void commit_config_to_fs(int sim) //overwrite specific data depending on simulator //1: simMetro, 2: lokSim3D
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

    size_t mcp_buf_size = getFilesize(LittleFS, "/mcp.txt");
    size_t pcf_buf_size = getFilesize(LittleFS, "/pcf.txt");

    char *current_mcp_config = new char[mcp_buf_size];    //17 + mcp_count * 16 * 10 + 1]; //23: length of header, ic_count * 16 pins * 8chars/pin (i2cadr(2) ";" pin "; &address ";" io "\n") + null-terminator
    char *current_pcf_config = new char[pcf_buf_size];            //14 + pcf_count * 5 * 9 + 1]; 

    readFile(LittleFS, "/mcp.txt", current_mcp_config, mcp_buf_size);
    readFile(LittleFS, "/pcf.txt", current_pcf_config, pcf_buf_size);

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
            //1. determin length of row in csv file in bytes
            //2. create buffer with this length
            //3. fill buffer with data from mcp_list
            //
            //4. append buffer to file 
            //
            if(sim == 1) //simmetro
            {
              int len = snprintf(NULL, 0, "%u;%u;%s;%u;%s;%s;%s\n", sizeof(uint8_t), sizeof(uint8_t), mcp_list[i].address[t], sizeof(uint8_t), mcp_list[i].address[t], mcp_list[i].address[t],  mcp_list[i].info[t]) + 1;
              buffer = new char[len];
              snprintf(buffer, len, "%u;%u;%s;%u;%s;%s;%s\n", mcp_list[i].i2c, t, mcp_list[i].address[t], (CHECK_BIT(mcp_list[i].portMode, t)) ? 1:0, mcp_list[i].address[t], mcp_list[i].address[t], mcp_list[i].info[t]);
            
            }
            else if(sim == 2) {}
            else{
              USBSerial.println("speicher fehlgeschlagen. ");
              return;
            }
            appendFile(LittleFS, "/mcp.txt", buffer);
            //USBSerial.printf("%s\n", buffer); //debugging purposes
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
          
            int len = snprintf(NULL, 0, "%u;%u;%s;%s;%s;%s\n", sizeof(uint8_t), sizeof(uint8_t), pcf_list[i].address[t], pcf_list[i].address[t], pcf_list[i].address[t], pcf_list[i].info[t]) + 1; //+1 for null-terminator
            buffer = new char[len];
            snprintf(buffer, len, "%u;%u;%s;%s;%s;%s\n", pcf_list[i].i2c, t, pcf_list[i].address[t], pcf_list[i].address[t], pcf_list[i].address[t], pcf_list[i].info[t]);
            appendFile(LittleFS, "/pcf.txt", buffer);
            //USBSerial.printf("%s\n", buffer); 
          
        }
      }
    }

    USBSerial.print(F("\nKonfigurationen erfolgreich gespeichert."));
    
    delete[] current_mcp_config;
    delete[] current_pcf_config;
  }
  delete[] buffer;

}
//==========================================================================================================================
//let user enter a number with specific bounds and max 16 digits
//==========================================================================================================================

//==========================================================================================================================
//choose which interface should be loaded after reboot
//==========================================================================================================================
void choose_sim() 
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
      delay(2);
    }
  }
  delay(300);

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
//
//======================================================================

void find_and_print_partitions() {
  bool found_coredump = false;

  USBSerial.println("\n\n----------------Partition---------------\n");
      
  esp_partition_iterator_t it;
  it = esp_partition_find(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, NULL);

  // Loop through all matching partitions, in this case, all with the type 'data' until partition with desired 
  // label is found. Verify if its the same instance as the one found before.
  for (; it != NULL; it = esp_partition_next(it)) {
      const esp_partition_t *part = esp_partition_get(it);
      USBSerial.printf("found partition '%s' at offset 0x%x with size 0x%x\n", part->label, part->address, part->size);
  }
  esp_partition_iterator_release(it);
  USBSerial.println();
  it = esp_partition_find(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, NULL);

  // Loop through all matching partitions, in this case, all with the type 'data' until partition with desired 
  // label is found. Verify if its the same instance as the one found before.
    for (; it != NULL; it = esp_partition_next(it)) {
      const esp_partition_t *part = esp_partition_get(it);
      USBSerial.printf("found partition '%s' at offset 0x%x with size 0x%x\n", part->label, part->address, part->size);
      if (strncmp(part->label, "coredump", 8) == 0) found_coredump = true;
    }
    if (!found_coredump) ESP.restart();

  // Release the partition iterator to release memory allocated for it
  esp_partition_iterator_release(it);
  USBSerial.println("\n----------------End of Partitions---------------\n\n\n\n");
}