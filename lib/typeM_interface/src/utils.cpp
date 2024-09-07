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
//==========================================================================================================================
//=======Helper functions for commit_config_to_fs (option 7)================================================================

static void update_mcp_config(char *current_config, int sim, size_t config_size)
{
  //temporarily saves a row of the config
  char *buffer;
  //story updated config in updated_config -> only overwrite current config file, if nothing went wrong 
  char *updated_config = new char[config_size]; 
  memset(&updated_config[0], '\0', config_size);
  const char *mcp_csvHeader = "i2c;pin;kanal;io;key;adresse;info\n";
  int len;

  strcpy(updated_config, mcp_csvHeader);
  //extract values from config file, copy to buffer except "kanal", "adresse" and "key" entry, depending on value of "sim"
                                  //i2c;pin;kanal;io;key;adresse;info
  CSV_Parser mcp_parse(current_config, "ucucsucss-" /*uc: unsigned char, s: string, -: unused*/, /*has header*/true, /*custom delimiter*/';');    
    
  uint8_t *i2c = (uint8_t*)mcp_parse["i2c"];
  uint8_t *pin = (uint8_t*)mcp_parse["pin"];
  char **kanal = (char**)mcp_parse["kanal"]; //tcp: here used for outputs (leds and stuff)
  uint8_t *io = (uint8_t*)mcp_parse["io"];
  char **key = (char**)mcp_parse["key"];        //used for inputs (buttons, switches, etc.), but also saved in mcp_list[].address[]
  char **address = (char**)mcp_parse["adresse"]; //tcp: here used for outputs (leds and stuff)
    
  USBSerial.print(F("\n Neue Konfiguration wird erstellt. Bitte warten..."));
  for (int t = 0; t < mcp_parse.getRowsCount(); t++) //update mcp.txt
  {
    //1. determin length of each row in bytes 
    //2. create buffer with this length
    //3. fill buffer with data
    //5. append buffer to file 

    if(sim == 1) //thmSim: copy "key" and "address" from mcp_parse, copy "kanal" from mcp_list
    {                   //i2c;pin;kanal;io;key;adresse;info
      char *_kanal = mcp_list[ i2c[t] - MCP_I2C_BASE_ADDRESS ].address[ pin[t] ];
      len = snprintf(NULL, 0, "%u;%u;%s;%u;%s;%s;%s\n", i2c[t], pin[t], _kanal, io[t], key[t], address[t],  mcp_list[ i2c[t] ].info[ pin[t] ] ) + 1; //+1 for null-terminator
      buffer = new char[len];
      snprintf(buffer, len, "%u;%u;%s;%u;%s;%s;%s\n", i2c[t], pin[t], _kanal, io[t], key[t], address[t],  mcp_list[ i2c[t] ].info[ pin[t] ] );
    }
    else if(sim == 2) 
    {                   //i2c;pin;kanal;io;key;adresse;info
      //store mcp_list[].address[] in "key"entry or in "adresse"-entry, depending on where the entry was stored in the config file
      if (strlen(key[t]) >= 1) 
      {
        char *_key = mcp_list[ i2c[t] - MCP_I2C_BASE_ADDRESS ].address[ pin[t] ];
        len = snprintf(NULL, 0, "%u;%u;%s;%u;%s;%s;%s\n", i2c[t], pin[t], kanal[t], io[t], _key, "", mcp_list[ i2c[t] ].info[ pin[t] ] ) + 1; //+1 for null-terminator
        buffer = new char[len];
        snprintf(buffer, len, "%u;%u;%s;%u;%s;%s;%s\n", i2c[t], pin[t], kanal[t], io[t], _key, "", mcp_list[ i2c[t] ].info[ pin[t] ] ); 
      }
      else if (strlen(address[t]) >= 1) 
      {
        char *_adr = mcp_list[ i2c[t] - MCP_I2C_BASE_ADDRESS ].address[ pin[t] ];
        len = snprintf(NULL, 0, "%u;%u;%s;%u;%s;%s;%s\n", i2c[t], pin[t], kanal[t], io[t], "", _adr, mcp_list[ i2c[t] ].info[ pin[t] ] ) + 1; //+1 for null-terminator
        buffer = new char[len];
        snprintf(buffer, len, "%u;%u;%s;%u;%s;%s;%s\n", i2c[t], pin[t], kanal[t], io[t], "", _adr, mcp_list[ i2c[t] ].info[ pin[t] ] ); 
      }
      else 
      {
        if (VERBOSE) USBSerial.println("Kein \"key\" oder \"adresse\" Eintrag gefunden. Alter Wert wird übernommen.");
        len = snprintf(NULL, 0, "%u;%u;%s;%u;%s;%s;%s\n", i2c[t], pin[t], kanal[t], io[t], key[t], address[t], mcp_list[ i2c[t] ].info[ pin[t] ] ) + 1; //+1 for null-terminator
        buffer = new char[len];
        snprintf(buffer, len, "%u;%u;%s;%u;%s;%s;%s\n", i2c[t], pin[t], kanal[t], io[t], key[t], address[t], mcp_list[ i2c[t] ].info[ pin[t] ] );
      }
      //store row
      strncpy(updated_config, buffer, len);
      if (VERBOSE) USBSerial.printf("%s\n", buffer); 
    }
  }
  delete[] buffer;

  //overwrite config file
  USBSerial.println("Alte Konfiguration wird überschrieben. Bitte warten...");
  clearFile(LittleFS, "/mcp.txt");

  //dont update file at once but row for row, because it could overload the ram which would lead to errors
  appendFile(LittleFS, "/mcp.txt", mcp_csvHeader);
  char *row = strtok(updated_config, "\n"); 
  while (row != NULL) {
    appendFile(LittleFS, "/mcp.txt", row);
    row = strtok(NULL, "\n"); //extract next row
  }
}
//==========================================================================================================================
static void update_pcf_config(char* current_config, int sim, size_t config_size)
{
  //temporarily saves a row of the config
  char *buffer;
  //story updated config in updated_config -> only overwrite current config file, if nothing went wrong 
  char *updated_config = new char[config_size]; 
  memset(&updated_config[0], '\0', config_size);
  const char *pcf_csvHeader = "i2c;pin;kanal;key;adresse;info\n";
  int len; //char-count of a row

  strcpy(updated_config, pcf_csvHeader);
  //extract values from config file, copy to buffer except "kanal", "adresse" and "key" entry, depending on value of "sim"
  //i2c;pin;kanal;key;adresse;info
  CSV_Parser pcf_parse(current_config, "ucucsss-" /*uc: unsigned char, s: string, -: unused*/, /*has header*/true, /*custom delimiter*/';');    
    
  uint8_t *i2c = (uint8_t*)pcf_parse["i2c"];
  uint8_t *pin = (uint8_t*)pcf_parse["pin"];
  char **kanal = (char**)pcf_parse["kanal"]; //tcp: here used for outputs (leds and stuff)
  char **key = (char**)pcf_parse["key"];        //used for inputs (buttons, switches, etc.), but also saved in mcp_list[].address[]
  char **address = (char**)pcf_parse["adresse"]; //tcp: here used for outputs (leds and stuff)
    
  USBSerial.print(F("\n Neue Konfiguration wird erstellt. Bitte warten..."));
  for (int t = 0; t < pcf_parse.getRowsCount(); t++) //update mcp.txt
  {
    //1. determin length of each row in bytes 
    //2. create buffer with this length
    //3. fill buffer with data
    //5. append buffer to file 

    if(sim == 1) //thmSim: copy "key" and "address" from mcp_parse, copy "kanal" from mcp_list
    {                   //i2c;pin;kanal;io;key;adresse;info
      char *_kanal = pcf_list[ i2c[t] - PCF_I2C_BASE_ADDRESS ].address[ pin[t] ];
      len = snprintf(NULL, 0, "%u;%u;%s;%s;%s;%s\n", i2c[t], pin[t], _kanal, key[t], address[t],  pcf_list[ i2c[t] ].info[ pin[t] ] ) + 1; //+1 for null-terminator
      buffer = new char[len];
      snprintf(buffer, len, "%u;%u;%s;%s;%s;%s\n", i2c[t], pin[t], _kanal, key[t], address[t],  pcf_list[ i2c[t] ].info[ pin[t] ] );
    }
    else if(sim == 2) 
    {                   //i2c;pin;kanal;io;key;adresse;info
      //store mcp_list[].address[] in "key"entry or in "adresse"-entry, depending on where the entry was stored in the config file
      if (strlen(key[t]) >= 1) 
      {
        char *_key = pcf_list[ i2c[t] - PCF_I2C_BASE_ADDRESS ].address[ pin[t] ];
        len = snprintf(NULL, 0, "%u;%u;%s;%s;%s;%s\n", i2c[t], pin[t], kanal[t], _key, "", pcf_list[ i2c[t] ].info[ pin[t] ] ) + 1; //+1 for null-terminator
        buffer = new char[len];
        snprintf(buffer, len, "%u;%u;%s;%s;%s;%s\n", i2c[t], pin[t], kanal[t], _key, "", pcf_list[ i2c[t] ].info[ pin[t] ] );
      }
      else if (strlen(address[t]) >= 1) 
      {
        char *_adr = pcf_list[ i2c[t] - PCF_I2C_BASE_ADDRESS ].address[ pin[t] ];
        len = snprintf(NULL, 0, "%u;%u;%s;%s;%s;%s\n", i2c[t], pin[t], kanal[t], "", _adr, pcf_list[ i2c[t] ].info[ pin[t] ] ) + 1; //+1 for null-terminator
        buffer = new char[len];
        snprintf(buffer, len, "%u;%u;%s;%s;%s;%s\n", i2c[t], pin[t], kanal[t], "", _adr, pcf_list[ i2c[t] ].info[ pin[t] ] );
      }
      else 
      {
        if (VERBOSE) USBSerial.println("Kein \"key\" oder \"adresse\" Eintrag gefunden. Alter Wert wird übernommen.");
        len = snprintf(NULL, 0, "%u;%u;%s;%s;%s;%s\n", i2c[t], pin[t], kanal[t], key[t], address[t], mcp_list[ i2c[t] ].info[ pin[t] ] ) + 1; //+1 for null-terminator
        buffer = new char[len];
        snprintf(buffer, len, "%u;%u;%s;%s;%s;%s\n", i2c[t], pin[t], kanal[t], key[t], address[t], mcp_list[ i2c[t] ].info[ pin[t] ] );
      }
      //store row
      strncpy(updated_config, buffer, len);
      if (VERBOSE) USBSerial.printf("%s\n", buffer);   
    }
  }
  delete[] buffer;

  //overwrite config file
  USBSerial.println("Alte Konfiguration wird überschrieben. Bitte warten...");
  clearFile(LittleFS, "/pcf.txt");
  
  //dont update file at once but row for row, because it could overload the ram which would lead to errors
  appendFile(LittleFS, "/pcf.txt", pcf_csvHeader);
  char *row = strtok(updated_config, "\n"); 
  while (row != NULL) {
    appendFile(LittleFS, "/pcf.txt", row);
    row = strtok(NULL, "\n"); //extract next row
  }
}

void commit_config_to_fs(int sim) //overwrite specific data depending on simulator //1: thmSim, 2: lokSim3D
{ 
  if (sim != 1 && sim != 2) { 
    USBSerial.printf("%i ist keine Option für \"commit_config_to_fs(int sim)\" ", sim);
    return;
  }

  char buffer[2] = {'\0', '\0'};
  USBSerial.print(F("\"S\" - Um die Änderungen im Flash zu sichern und die alte Konfiguration zu überschreiben, geben Sie ein \"S\" ein."));
  USBSerial.print(F("\nKehren Sie mit \"C\" zurück zum Menü."));   
  
  while(USBSerial.available() < 1) { vTaskDelay(10); }
  USBSerial.readBytesUntil('\n', buffer, 1);
  USBSerial.println(buffer);

  if (buffer[0] == 'C') return;

  else if (buffer[0]  == 'S')
  {
    size_t mcp_buf_size = getFilesize(LittleFS, "/mcp.txt");
    size_t pcf_buf_size = getFilesize(LittleFS, "/pcf.txt");

    char *current_mcp_config = new char[mcp_buf_size];    
    char *current_pcf_config = new char[pcf_buf_size];            

    readFile(LittleFS, "/mcp.txt", current_mcp_config, mcp_buf_size);
    readFile(LittleFS, "/pcf.txt", current_pcf_config, pcf_buf_size);
    
    //=========================================================
    
    USBSerial.print(F("\nAktualisiere mcp.txt..."));
    update_mcp_config(current_mcp_config, sim, mcp_buf_size);
  
    USBSerial.print(F("\nAktualisiere pcf.txt..."));
    update_pcf_config(current_pcf_config, sim, pcf_buf_size);

    //=========================================================

    USBSerial.print(F("\nAktualisierung erfolgreich abgeschlossen."));
    delete[] current_mcp_config;
    delete[] current_pcf_config;
  }

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
  USBSerial.print(F("Welche Schnittstelle soll nach dem nächsten Neustart geladen werden?\n\n1 -> THM-Simulator\n2 -> LokSim3D\n"));

  strcpy(info,  "\nDiese Option steht nicht zur verfügung.\nGeben Sie eine \"1\" oder eine \"2\" ein, oder kehren Sie mit \"C\" zurück zum Menü.\n");
  if (get_integer_with_range_check(&option, 1, 2, info)) return;
  
  preferences.begin("APP", false, "nvs");

  if (option == 1) 
  {
    preferences.putUInt("app_settings", option);
    
    USBSerial.print(F("\nSchnittstelle zu THM-Simulator wird dem nächsten Neustart initialisiert."));
  } 
  else if (option == 2) 
  {
    preferences.putUInt("app_settings", option);
    USBSerial.print(F("\nSchnittstelle zu LokSim3D wird dem nächsten Neustart initialisiert."));
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
