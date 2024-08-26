#include "trainSim_interface/config_file_utils.h"

using namespace trainSim_interface;
  
namespace trainSim_config{
  

bool load_config()
{
  if(!LittleFS.begin()) 
  {
    USBSerial.print(F("\nLittleFS Mount fehlgeschlagen.\nKonfiguration konnte nicht geladen werden. Setup abgebrochen...\n"));
    return 0;
  }
  
  uint8_t i, t; //lokal for loop counter
  uint8_t mcp_count = 0, pcf_count = 0; //gets printed out-> user can see, how many ICs of one type will be initialized

  USBSerial.printf("\nDateigröße wird ermittelt: %s", "/mcp.txt");
  size_t mcp_buf_size = getFilesize(LittleFS, "/mcp.txt");

  USBSerial.printf("\nDateigröße wird ermittelt: %s\n", "/pcf.txt");
  size_t pcf_buf_size = getFilesize(LittleFS, "/pcf.txt");

  if (!mcp_buf_size || !pcf_buf_size) {
    return 0;
  }

  USBSerial.printf("\nmcp_buf_size: %i\npcf_buf_size: %i\n\n", mcp_buf_size, pcf_buf_size);

  char *mcp_buf = new char[mcp_buf_size]; //char *mcp_buf = new char[mcp_buf_size];
  char *pcf_buf = new char[pcf_buf_size];

  for (i = 0; i < MAX_IC_COUNT; i++) //reset every mcp struct-object
  {
    for (t = 0; t < 16; t++)
    {
      mcp_list[i].address[t] = new char[3];//allocate memory for 2 chars + null-terminator
      strcpy(mcp_list[i].address[t], "-1");
    }
  }

  for (i = 0; i < MAX_IC_COUNT; i++) //reset every pcf struct-object
  {
    for (t = 0; t < 5; t++)
    {
      pcf_list[i].address[t] = new char[3];//allocate memory for 2 chars + null-terminator
      strcpy(pcf_list[i].address[t], "-1");
    }
  }
  
  readFile(LittleFS, "/mcp.txt", mcp_buf, mcp_buf_size);
  readFile(LittleFS, "/pcf.txt", pcf_buf, pcf_buf_size);

  CSV_Parser mcp_parse(mcp_buf, "ucucsuc" /*uc: unsigned char, s: string*/, /*has header*/true, /*custom delimiter*/';');  
  
  USBSerial.println();
  mcp_parse.print();
  
  uint8_t *i2c = (uint8_t*)mcp_parse["i2c"];
  uint8_t *pin = (uint8_t*)mcp_parse["pin"];
  char **address = (char**)mcp_parse["kanal"];
  uint8_t *io = (uint8_t*)mcp_parse["io"];
  

  for (t = 0; t < mcp_parse.getRowsCount(); t++) 
  {
    if (pin[t] < 0 || pin[t] > 15) //
    {
      USBSerial.printf("\nPin liegt nicht zwischen 0 und 15: %u", pin[t]);
      strcpy(mcp_list[i2c[t] - MCP_I2C_BASE_ADDRESS].address[pin[t]], "-1");      
    }
    else if (strcmp(address[t], "-1") == 0) 
    {
      USBSerial.printf("\nPin %u an IC mit Adresse %i (DEC) nicht in Verwendung.", pin[t], i2c[t]);
    }
    else {
      //store address from config file in mcp_list and "connect" every given address with specific pin in the struct.
      //e.g. current column: 22, 02, 03, 1 (i2c, pin, address, io) -> strcpy( mcp_list[22-20].address[2], 03 );
      strcpy(mcp_list[i2c[t] - MCP_I2C_BASE_ADDRESS].address[pin[t]], address[t]);  
      bitWrite(mcp_list[i2c[t] - MCP_I2C_BASE_ADDRESS].portMode, pin[t], io[t]);
      

      //custom buttons, status affects the data, that will be transmitted after change of the combined throttle
      if (strcmp(address[t], "ac") == 0) {
        acceleration_button[0] = i2c[t];
        acceleration_button[1] = pin[t];
      }
      else if (strcmp(address[t], "dc") == 0) {
        deceleration_button[0] = i2c[t];
        deceleration_button[1] = pin[t];
      }
      
    }

     if (t == mcp_parse.getRowsCount()-1) //end of file reached
    { 
      mcp_count++;
      mcp_list[i2c[t] - MCP_I2C_BASE_ADDRESS].enabled = true;
      break;
    } else if (i2c[t] != i2c[t+1])  { 
        mcp_count++;
        mcp_list[i2c[t] - MCP_I2C_BASE_ADDRESS].enabled = true;
      }  
  } 

  USBSerial.printf("mcp_count: %i\n", mcp_count);

  USBSerial.print(F("\nLese Konfigurationsdatei \"pcf.txt\"...\n"));
  
  CSV_Parser pcf_parse(pcf_buf, "ucucs" /*uc: unsigned char, s: string*/, /*has header*/true, /*custom delimiter*/';');
 
  USBSerial.println();
  pcf_parse.print();

  i2c = (uint8_t*)pcf_parse["i2c"];
  pin = (uint8_t*)pcf_parse["pin"];
  address = (char**)pcf_parse["kanal"];

  for (t = 0; t < pcf_parse.getRowsCount(); t++)
  {
    if (pin[t] < 0 || pin[t] > 4) //
    {
      USBSerial.printf("\nPin liegt nicht zwischen 0 und 4: %u", pin[t]);
      strcpy(pcf_list[i2c[t]-PCF_I2C_BASE_ADDRESS].address[pin[t]], "-1");   
    }
    else if (strcmp(address[t], "-1") == 0) 
    {
      USBSerial.printf("\nPin %u an IC mit Adresse %i (DEC) nicht in verwendung.", pin[t], i2c[t]);
    }
    else {
      strcpy(pcf_list[i2c[t] - PCF_I2C_BASE_ADDRESS].address[pin[t]], address[t]); 

      //custom address for combined throttle, where direction is indicated by two buttons 
      //for seperated actuators use regular configuration with specific address
      if (strcmp(address[t], "ct") == 0) {
        combined_throttle_pin[0] = i2c[t];
        combined_throttle_pin[1] = pin[t]; 
      }
        
    }

    if (t == pcf_parse.getRowsCount() - 1)
    {
      pcf_count++;
      pcf_list[i2c[t] - PCF_I2C_BASE_ADDRESS].enabled = true;
      break;
    } else if (i2c[t] != i2c[t+1]) 
    {
      pcf_count++;
      pcf_list[i2c[t] - PCF_I2C_BASE_ADDRESS].enabled = true;
    }
  }
  
  USBSerial.printf("pcf_count: %i\n\n", pcf_count);


  for (i = 0; i < MAX_IC_COUNT; i++) 
  {
    mcp_list[i].i2c = i + MCP_I2C_BASE_ADDRESS;
    mcp_list[i].mcp = MCP23017();
    mcp_list[i].last_reading = mcp_list[i].portMode;

    if(!mcp_list[i].enabled) { //no entry in csv file for this i2c address found
      continue;
    }
      
    if (!mcp_list[i].mcp.init(mcp_list[i].i2c)) {
      USBSerial.printf("\nMCP23017 mit der Adresse 0x%i konnte nicht initialisiert werden.", mcp_list[i].i2c);
      mcp_list[i].enabled = false;
    }

    mcp_list[i].mcp.portMode(MCP23017Port::A, mcp_list[i].portMode & 0xFF); //input_pullups enabled by default, portA: LSB
    mcp_list[i].mcp.portMode(MCP23017Port::B, (mcp_list[i].portMode >> 8) & 0xFF); //portB: MSB
    mcp_list[i].mcp.writeRegister(MCP23017Register::GPIO_A, 0x00);
    mcp_list[i].mcp.writeRegister(MCP23017Register::GPIO_B, 0x00);  //Reset ports
    mcp_list[i].mcp.writeRegister(MCP23017Register::IPOL_A, 0x00);    
    mcp_list[i].mcp.writeRegister(MCP23017Register::IPOL_B, 0x00);  //If a bit is set, the corresponding GPIO register bit will reflect the inverted value on the pin                                                               
  }
    
  for (i = 0; i < MAX_IC_COUNT; i++) 
  { 
    pcf_list[i].i2c = i + PCF_I2C_BASE_ADDRESS;
    pcf_list[i].pcf = Adafruit_PCF8591();

    if(!pcf_list[i].enabled) { //no entry in csv file for this i2c address found
      continue;
    }
   
    if (!pcf_list[i].pcf.begin(pcf_list[i].i2c)) {
      USBSerial.printf("\n\nPCF8591 mit der Adresse 0x%x konnte nicht initialisiert werden.", pcf_list[i].i2c);
      pcf_list[i].enabled = false;
    } else {
      pcf_list[i].pcf.enableDAC(true);
      pcf_list[i].pcf.analogWrite(0);
      for (t = 0; t < 4; t++) {
        pcf_list[i].last_reading[t] = pcf_list[i].pcf.analogRead(t);
      }
    }
  } 

  delete[] pcf_buf;
  delete[] mcp_buf; 

  return 1;
}

/*
==========================================================================================================================
==========================================================================================================================
==========================================================================================================================
==========================================================================================================================

//helper-functions for opt7() to store current config in flash

*/

static void mcp_Config2CsvString(uint8_t i2c_adr, uint8_t pin, char *buffer) 
{

  char data[3];
  memset(&data[0], '\0', 3);
  memset(&buffer[0], '\0', 15); //clear buffer

  sprintf(data, "%d", i2c_adr);
  strcat(buffer,  data);
  strcat(buffer, ";");

  sprintf(data, "%d", pin);
  strcat(buffer, data);
  strcat(buffer, ";");

  strcat(buffer, mcp_list[i2c_adr-32].address[pin]);
  strcat(buffer, ";");

  sprintf(data, "%d", (CHECK_BIT(mcp_list[i2c_adr-MCP_I2C_BASE_ADDRESS].portMode, pin)) ? 1:0 );
  strcat(buffer, data);
  strcat(buffer, "\n");

}

//==========================================================================================================================
//==========================================================================================================================

static void pcf_Config2CsvString(uint8_t i2c_adr, uint8_t pin, char *buffer) 
{
  
  char data[3] = {'\0'};
  memset(&buffer[0], '\0', 15); //clear buffer

  sprintf(data, "%d", i2c_adr);
  strcat(buffer,  data);
  strcat(buffer, ";");

  sprintf(data, "%d", pin);
  strcat(buffer, data);
  strcat(buffer, ";");

  strcat(buffer, pcf_list[i2c_adr-72].address[pin]);
  strcat(buffer, ";");
  strcat(buffer, "\n");

}

//==========================================================================================================================
//print io configuration in binary format
//==========================================================================================================================

static void printBinary(uint16_t value) 
{
    
    for (int t = 0; t < 16; t++) { //print portMode in binary format
      USBSerial.printf("%i", (CHECK_BIT(value, t)) ? 1 : 0 ); //at USBSerial monitor, bit 0 represents pin 0 and bit 15 pin 15
    }
    USBSerial.println();
}

//==========================================================================================================================
//let user enter a number with specific bounds
//==========================================================================================================================

static bool process_UserInput(char* buffer, uint8_t *var, int length, uint lower_bound, uint upper_bound, char *info) //returns true (exit to menu) if user entered "C"
{  
  memset(&buffer[0], '\0', length); 
  //USBSerial.read(); 
  while (USBSerial.available()) USBSerial.read();

  while(!USBSerial.available()) {vTaskDelay(1);} //wait for user-input
  if (USBSerial.peek() == 'C') return true;     

  int bytesRead = 0;
  while (USBSerial.available()) 
  {
    bytesRead += USBSerial.readBytesUntil('\n', buffer + bytesRead, length - bytesRead);
    if (bytesRead > 0 || buffer[0] == '\n') break;
    vTaskDelay(1); 
  }

  buffer[bytesRead] = '\0'; // String korrekt terminieren
  USBSerial.printf("\n%s", buffer);

  int error = sscanf(buffer, "%u", var);
  USBSerial.printf("\nerror: %i", error);

  while (error != 1 || *var < lower_bound || *var > upper_bound) 
  {
    USBSerial.printf("\nIhre Eingabe: %s", buffer);
    memset(buffer, '\0', length + 1); // Puffer leeren
    USBSerial.printf("\n%s\n", info);

    while (USBSerial.available()) USBSerial.read();

    while (!USBSerial.available()) vTaskDelay(1); //wait for userinput
        
    if (USBSerial.peek() == 'C') return true;
        
    bytesRead = 0;
    int bytesRead = 0;
    while (USBSerial.available()) 
    {
      bytesRead += USBSerial.readBytesUntil('\n', buffer + bytesRead, length - bytesRead);
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

//==========================================================================================================================
//resume input tasks and print out infos for the user
//==========================================================================================================================

static void opt_1() 
{
    config_menu = 1;  //set flag to return to menu after this function
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
//print current config in CSV style and go back to menu
//==========================================================================================================================

static void opt_2() 
{
    int i, t;
    config_menu = 1;

    USBSerial.print(F("Die Ausgabe erfolgt sortiert nach der I2C-Adresse (DEC nicht HEX!).\nEs werden erst alle MCP-konfigurationen gelistet gefolgt von denen der PCF-ICs.\n"));

    USBSerial.print(F("\nmcp-Konfiguration:\n\ni2c;pin;kanalnummer;io"));
    
     for (i = 0; i < MAX_IC_COUNT; i++) 
     {
      if(!mcp_list[i].enabled) continue;//i2c adress not in use
      for (t = 0; t < 16; t++)  
      { 
        USBSerial.printf("\n%i;", mcp_list[i].i2c);
        USBSerial.printf("%i;", t);
        USBSerial.printf("%s;", mcp_list[i].address[t]);
        USBSerial.printf("%i", (mcp_list[i].portMode & (1 << (t)))? 1:0); //check if bit is set a position t
            
      }
    }

    USBSerial.print(F("\n\npcf-Konfiguration:\n\ni2c;pin;kanalnummer"));

    for (i = 0; i < MAX_IC_COUNT; i++) 
    {
      if(!pcf_list[i].enabled) continue;//i2c adress not in use
      for (t = 0; t < 5; t++) 
      {
        USBSerial.printf("\n%i;", pcf_list[i].i2c);
        USBSerial.printf("%i;", t);
        USBSerial.printf("%s;", pcf_list[i].address[t]);
      }
    }
    USBSerial.print(F("\n\nMenü wird erneut aufgerufen..."));
    vTaskDelay(pdMS_TO_TICKS(1000));
}

//==========================================================================================================================
//Change portConfig or single pin config of a specific MCP IC 
//==========================================================================================================================

static void opt_3() 
{
  int i, t;
  uint8_t i2c_adr, pin;
  char *buffer = new char[17]{'\0'}; //16bits + null-terminator
  char info[250];        //info for user, if input didnt fit to request, static array, because there is enough memory, but memory allocation costs cpu cycles
  uint8_t option;

  USBSerial.print(F("\nSie können jederzeit zum Menü zurückgelangen. Geben Sie dazu ein \"C\" ein."));   

  USBSerial.printf("\nGeben Sie die I2C-Addresse (%i bis %i) des MCP-ICs ein, dessen Port-Konfiguration Sie ändern möchten.", MCP_I2C_BASE_ADDRESS, MCP_I2C_END_ADDRESS);  
  strcpy(info, "\nDiese Adresse steht nicht zur verfügung.\nGeben Sie eine passende Adresse ein, oder kehren Sie mit \"C\" zurück zum Menü.\n");
  if (process_UserInput(buffer, &i2c_adr, 2, MCP_I2C_BASE_ADDRESS, MCP_I2C_END_ADDRESS, info)) return; 

  USBSerial.print(F("\n\nAktuelle Portkonfiguration:\n")); 
  for (i = 0; i < MAX_IC_COUNT; i++) {
    if (mcp_list[i].i2c == i2c_adr) break;     
  }
  printBinary(mcp_list[i].portMode);

  memset(&buffer[0], '\0', 17); //clear buffer
  USBSerial.print(F("\n\nUm die Portkonfiguration dieses ICs zu ändern, geben Sie eine \"1\" ein. Um nur einen einzelnen Pin anzupassen, geben Sie eine \"2\" ein."));
  strcpy(info, "\nDiese Option steht nicht zur verfügung.\nGeben Sie eine \"1\" oder eine \"2\" ein, oder kehren Sie mit \"C\" zurück zum Menü.\n");
  if (process_UserInput(buffer, &option, 1, 1, 2, info)) return;

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
      mcp_list[i].portMode = (uint16_t)strtol(buffer, NULL, 2); //convert with base 2 instead of using portMode variable
    }    
    USBSerial.print(F("\nPortkonfiguration geändert zu: \n"));
    printBinary(mcp_list[i].portMode);
  }

  else if (option == 2) 
  {
    USBSerial.print(F("\nGeben sie eine Nummer von 0 bis 15 für den Pin ein, den Sie ändern möchten.\n"));  
    memset(&buffer[0], '\0', 17); //clear buffer
    strcpy(info, "\nDiesen Pin gibt es nicht\nGeben sie eine Nummer von 0 bis 15 für den Pin ein, den Sie ändern möchten oder kehren Sie mit \"C\" zurück zum Menü.\n");
    if (process_UserInput(buffer, &pin, 2, 0, 15, info)) return;
    USBSerial.printf("\nPin %i wird umgestellt von %i", pin, (CHECK_BIT(mcp_list[i].portMode, pin) ? 1:0));
    
    bitWrite(mcp_list[i].portMode, pin, !CHECK_BIT(mcp_list[i].portMode, pin)); //toggle bit

  }
  //mcp_list[i].mcp.writeRegister(MCP23017Register::GPIO_B, 0x00);  //Reset ports
 
  USBSerial.printf(" auf %i\n", (CHECK_BIT(mcp_list[i].portMode, pin) ? 1:0));

  USBSerial.printf("mcp_list[i].portMode: ");
  printBinary(mcp_list[i].portMode);

  USBSerial.printf("Setting portMode for Port A: ");
  printBinary(mcp_list[i].portMode & 0x00FF);

  if (xSemaphoreTake(i2c_mutex, portMAX_DELAY) == pdTRUE) 
  { 
          
    mcp_list[i].mcp.writeRegister(MCP23017Register::GPIO_A, 0x00);
    mcp_list[i].mcp.portMode(MCP23017Port::A, mcp_list[i].portMode & 0x00FF); //input_pullups enabled by default, portA: LSB
    USBSerial.println("portA set");

    vTaskDelay(1);

    USBSerial.printf("\nSetting portMode for Port B: ");
    printBinary((mcp_list[i].portMode >> 8) & 0x00FF);

    mcp_list[i].mcp.writeRegister(MCP23017Register::GPIO_B, 0x00);
    mcp_list[i].mcp.portMode(MCP23017Port::B, (mcp_list[i].portMode >> 8) & 0x00FF); //portB: MSB
    USBSerial.println("portB set");
    
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
  if (strcmp(buffer, "j") == 0) config_menu = 1;
  delete[] buffer;
  
}

//==========================================================================================================================
//change address of a singel IO (I2C address & pin number needed)
//==========================================================================================================================

static void opt_4() 
{
  uint8_t i, t;
  uint8_t i2c_adr;
  uint8_t pin;
  uint8_t address;
  char *buffer = new char[3]{'\0'};     //buffer for user-input 
  char current_address[3] = {'\0'};        
  char info[250] = {'\0'};               //info for user, if input didnt fit to request, static array, because there is enough memory, but memory allocation costs cpu cycles


  USBSerial.print(F("Sie können jederzeit zum Menü zurückgelangen. Geben Sie dazu ein \"C\" ein."));
  USBSerial.printf("\nGeben Sie die I2C-Addresse des ICs ein, dessen Port-Konfiguration Sie ändern möchten (%i-%i & %i-%i (DEC)): ", MCP_I2C_BASE_ADDRESS, MCP_I2C_END_ADDRESS, PCF_I2C_BASE_ADDRESS, PCF_I2C_END_ADDRESS);   
  strcpy(info, "\nDiese Adresse steht nicht zur verfügung.\nGeben Sie eine passende Adresse ein, oder kehren Sie mit \"C\" zurück zum Menü.\n");
  memset(&buffer[0], '\0', 3);
  while (1)  //let user input some valid i2c adress or "C" to return back to the menu. if user entered a valid address, search the index in the lists where this adress is lokated at
  {  
    if (process_UserInput(buffer, &i2c_adr, 2, MCP_I2C_BASE_ADDRESS, PCF_I2C_END_ADDRESS, info)) return;
    
    i = i2c_adr - MCP_I2C_BASE_ADDRESS; //assume, entered address belongs to an mcp ic, i = index in mcp_list 
    if (i < 9) {break;} else { i = i2c_adr - PCF_I2C_BASE_ADDRESS;} //yes? break while loop otherwhise check if it belongs to pcf ic (i = unsigned! cant be < 0)
    if (i < 9)  break; //yes? break, otherwhise go back to process_UserInput (user entered a number between 40 and 71)

    USBSerial.printf("%s", info);
  }

  USBSerial.print(F("\n\nGeben Sie den Pin ein, dessen Kanalnummer Sie ändern möchten."));
    
  if (i2c_adr < MCP_I2C_END_ADDRESS)  //mcp ic        //ask user, which pin he wants to change and then copy current address 
  {
    USBSerial.print(F("\nDabei entspricht eine Nummer von 0 bis 7 einem Pin an Port A. Höhere Nummern entsprechen einem Pin an Port B.\n Achtung! Es wird nicht überprüft, ob derselbe Kanal schon bei einem anderen I/O verwendet wird!\n")); 
    strcpy(info,  "\nDiesen Pin gibt es nicht\nGeben sie eine Nummer von 0 bis 15 für den Pin ein, den Sie ändern möchten oder kehren Sie mit \"C\" zurück zum Menü.\n");
    memset(&buffer[0], '\0', 3);
    if (process_UserInput(buffer, &pin, 2, 0, 15, info)) return; 

    strcpy(current_address, mcp_list[i].address[pin]);
    USBSerial.printf("\n\nAktuell ist für diesen I/O Kanal %s hinterlegt.\nGeben Sie jetzt eine neue Kanalnummer ein:", current_address);
    memset(&buffer[0], '\0', 3);
    strcpy(info,  "\nDiese Kanalnumer gibt es nicht\nGeben sie eine Nummer von 0 bis 99 ein oder kehren Sie mit \"C\" zurück zum Menü.\n");
    if (process_UserInput(buffer, &address, 2, 0, 99, info)) return;

    strncpy(mcp_list[i].address[pin], buffer, 2); //write new address to mcp list

  } else if (i2c_adr > PCF_I2C_BASE_ADDRESS) //pcf ic
  {
    USBSerial.print(F("\nDabei entspricht eine Nummer von 0 bis 3 einem der 4 analogen Eingänge. Eine 4 führt entsprechend zur Änderung des Kanals des analogen Ausgangs.\nAchtung! Es wird nicht überprüft, ob derselbe Kanal schon bei einem anderen I/O verwendet wird!\n"));
    memset(&buffer[0], '\0', 3);
    strcpy(info,  "\nDiesen Pin gibt es nicht\nGeben sie eine Nummer von 0 bis 4 für den Pin ein, den Sie ändern möchten oder kehren Sie mit \"C\" zurück zum Menü.\n");
    if (process_UserInput(buffer, &pin, 3, 0, 4, info)) return;
    strcpy(current_address, pcf_list[i].address[pin]);

    USBSerial.printf("\n\nAktuell ist für diesen I/O Kanal %s hinterlegt.\nGeben Sie jetzt eine neue Kanalnummer ein.", current_address);
    memset(&buffer[0], '\0', 3);
    strcpy(info,  "\nDiese Kanalnumer gibt es nicht\nGeben sie eine Nummer von 0 bis 99 ein oder kehren Sie mit \"C\" zurück zum Menü.\n");
    if (process_UserInput(buffer, &address, 2, 0, 99, info)) return;

    strcpy(pcf_list[i].address[pin], buffer);
  }
    
  USBSerial.print(F("\n\nKanalnummer erfolgreich aktualisiert.\n"));
  memset(&buffer[0], '\0', 3);
 
   //clear hw rx buffer
  while (USBSerial.available()) {USBSerial.read();}
  USBSerial.print(F("\nMenü erneut aufrufen? (j/n)"));
  USBSerial.print(F("\n(Eine andere Eingabe als j wird als Nein interpretiert)\n"));

  while(!USBSerial.available()){vTaskDelay(1);}
  memset(&buffer[0], '\0', 3); 
  USBSerial.readBytesUntil('\n', buffer, 2); 
  buffer[1] = '\0';
  USBSerial.printf("\n%s", buffer);
  if (strcmp(buffer, "j") == 0) config_menu = 1;
  delete[] buffer;
}

//==========================================================================================================================
//change number of ICs
//==========================================================================================================================

static void opt_5() 
{ 
  char *buffer = new char[3]{'\0'}; 
  char info[250];        //info for user, if input didnt fit to request, static array, because there is enough memory, but memory allocation costs cpu cycles
  int mcp_count = 0;
  int pcf_count = 0;

  uint8_t i, t, count;  //counter for number of free i2c addresses
  uint8_t new_mcp_count = 0, new_pcf_count = 0; //user input
  uint8_t option;  //M -> change num of mcp ics, P-> change num of pcf ics
  uint8_t enable_i2c_adr; //user enteres multiple addresses he wants to add;

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
  if (process_UserInput(buffer, &option, 1, 1, 2, info)) return;
  while (USBSerial.available() > 0) {USBSerial.read();}
  
  if (option == 1)
  {
    USBSerial.print(F("\nGeben Sie die gewünschte Zahl an Port Expandern an. (max. 8)\n"));
    strcpy(info, "\nDiese Zahl an Portexpandern ist nicht umsetzbar. Geben Sie eine Zahl von 0 bis 8 ein, oder kehren Sie mit \"C\" zurück zum Menü.");
    if (process_UserInput(buffer, &new_mcp_count, 1, 0, 8, info)) return;
    
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
        if (process_UserInput(buffer, &enable_i2c_adr, 2, MCP_I2C_BASE_ADDRESS, MCP_I2C_END_ADDRESS, info)) return;
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
          if (process_UserInput(buffer, &enable_i2c_adr, 1, MCP_I2C_BASE_ADDRESS, MCP_I2C_END_ADDRESS, info)) return;
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
    if (new_pcf_count > pcf_count) 
    {
      USBSerial.print(F("Freie I2C Adressen: "));
      for(i = 0; i < MAX_IC_COUNT; i++) 
      {
        if (pcf_list[i].enabled) continue;
        USBSerial.printf("%i (%02x), ", i + MCP_I2C_BASE_ADDRESS, i + MCP_I2C_BASE_ADDRESS);
        count++;
      }
      
      strcpy(info, "\nDiese Addresse kann nicht vergeben werden. Geben Sie eine Zahl von 72 bis 79 ein, oder kehren Sie mit \"C\" zurück zum Menü.");
      count = (new_pcf_count - pcf_count);
      t = 0;
      while (t < count) //give free addresses to "new" mcp ICs
      {
        USBSerial.printf("\nGeben Sie eine der freien Adressen ein, die sie für IC-Nr. %i vergeben möchten, oder kehren Sie mit \"C\" zurück zum Menü.\n", t+1);
        if (process_UserInput(buffer, &enable_i2c_adr, 2, PCF_I2C_BASE_ADDRESS, PCF_I2C_END_ADDRESS, info)) return;
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
    else if(new_pcf_count < pcf_count) //user wants to "delete" mcp ICs
    {
      for (i = new_pcf_count; i < pcf_count; i++) 
      {
        strcpy(info, "\nDiese Addresse gibt es nicht. Geben Sie eine Zahl von 72 bis 79 ein, oder kehren Sie mit \"C\" zurück zum Menü.");
        count = (pcf_count - new_pcf_count);
        t = 0;
        while (t < count) //give free addresses to "new" mcp ICs
        {
          USBSerial.print(F("\nGeben Sie die Adresse ein, die Sie deaktivieren möchten, oder kehren Sie mit \"C\" zurück zum Menü.\nKanalnummern und Portkonfiguration werden nicht zurückgesetzt.\nDeaktivierte ICs werden nicht im Flashspeicher gesichert."));
          if (process_UserInput(buffer, &enable_i2c_adr, 1, MCP_I2C_BASE_ADDRESS, MCP_I2C_END_ADDRESS, info)) return;
          for (i = 0; i < MAX_IC_COUNT; i++) //does userinput match a free address?
          {
            if (!pcf_list[i].enabled || pcf_list[i].i2c != enable_i2c_adr) continue;
            if (xSemaphoreTake(i2c_mutex, portMAX_DELAY) == pdTRUE) 
            {
              pcf_list[i].enabled = false;
              pcf_list[i].pcf.analogWrite(0);
              pcf_list[i].pcf.enableDAC(false);
            
              xSemaphoreGive(i2c_mutex);
            }
            t++;
            break;
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
  memset(&buffer[0], '\0', 3); 
  USBSerial.readBytesUntil('\n', buffer, 1); 
  buffer[1] = '\0';
  USBSerial.printf("\n%s", buffer);
  if (strcmp(buffer, "j") == 0) config_menu = 1;
  delete[] buffer;
}

//==========================================================================================================================
//toggle verbose output
//==========================================================================================================================

static void opt_6()  
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
    config_menu = 1;
}

//==========================================================================================================================
//esc from menu with option to safe changes to files
//==========================================================================================================================

static void opt_7()  
{ 

  char *buffer = new char[2]{'\0'};
  USBSerial.print(F("Um die Änderungen im Flash zu sichern und die alte Konfiguration zu überschreiben, geben Sie ein \"S\" ein."));
  USBSerial.print(F("\nOder kehren Sie mit \"C\" zurück zum Menü."));   
  
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
    char buffer[15];
    const char *mcp_csvHeader = "i2c;pin;kanal;io\n";
    const char *pcf_csvHeader = "i2c;pin;kanal\n";

    for (i = 0; i < MAX_IC_COUNT; i++) 
    {
      if (mcp_list[i + MCP_I2C_BASE_ADDRESS].enabled) mcp_count++;
      if (pcf_list[i + PCF_I2C_BASE_ADDRESS].enabled) pcf_count++;
    }

    char *mcp_config = new char[17 + mcp_count * 16 * 10 + 1]; //23: length of header, ic_count * 16 pins * 8chars/pin (i2cadr(2) ";" pin "; &address ";" io "\n") + null-terminator
    char *pcf_config = new char[14 + pcf_count * 5 * 9 + 1]; 

    USBSerial.print(F("\nKonfigurationen werden überschrieben. Bitte warten..."));

    USBSerial.print(F("Dateien gelöscht."));
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

          if (strcmp(mcp_list[i].address[t], "-1") != 0) 
          {
            mcp_Config2CsvString(mcp_list[i].i2c, t, buffer);
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
            mcp_Config2CsvString(mcp_list[i].i2c, t, buffer);
            appendFile(LittleFS, "/mcp.txt", buffer);
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
//opt 8: exit menu
//opt 9: restart device
//==========================================================================================================================

//==========================================================================================================================
//change variable in eeprom, which is used to determine which interface is used
//==========================================================================================================================

static void opt_10() 
{
  char *buffer = new char[2];  
  char info[250] = {'\0'};
  uint8_t option = 0;
   
  memset(&buffer[0], '\0', 2);

  USBSerial.print(F("Kehren Sie mit \"C\" zurück zum Menü.\n"));   
  USBSerial.print(F("Welche Schnittstelle soll nach dem nächsten Neustart geladen werden?\n\n1 -> TrainSim\n2 -> LokSim3D\n"));

  strcpy(info,  "\nDiese Option steht nicht zur verfügung.\nGeben Sie eine \"1\" oder eine \"2\" ein, oder kehren Sie mit \"C\" zurück zum Menü.\n");
  if (process_UserInput(buffer, &option, 1, 1, 2, info)) return;
  
  preferences.begin("APP", false, "nvs");

  if (option == 1) 
  {
    preferences.putUInt("app_setting", option);
    
    USBSerial.print(F("\nTrainSim Schnittstelle wird nach dem nächsten Neustart geladen."));
  } 
  else if (option == 2) 
  {
    preferences.putUInt("app_setting", option);
    USBSerial.print(F("\nLokSim3D Schnittstelle wird nach dem nächsten Neustart geladen."));
  }
 
  USBSerial.printf("In NVS gepeichert: %i", preferences.getUInt("app_setting", 0));
  preferences.end();

  if (option) config_menu = 1; //return to menu, where user can reset the device with option 9
  delete[] buffer;
}


//==========================================================================================================================
//esc from menu
//==========================================================================================================================
static void esc()  
{
  vTaskResume(Task1);
  vTaskResume(Task2);
  vTaskResume(Task4);
  vTaskResume(Task5);
  vTaskSuspend(NULL);
  
}
//==========================================================================================================================
//USBSerial Configuration Menu
//==========================================================================================================================

void serialConfigMenu()
{
    //suspend tasks to prevent data corruption
    vTaskSuspend(Task1);      //digital input task
    vTaskSuspend(Task2);      //analog input task
    vTaskSuspend(Task4);      //USBSerial rx task     
    vTaskSuspend(Task5);      //USBSerial tx task

    uint8_t option = 0;
    char info[145];
    char *buffer = new char[3]{'\0'}; //two chars and null-terminator

    while (USBSerial.available() > 0) {USBSerial.read();}

    config_menu = 0; 
     
    USBSerial.print(F("\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n"));
    USBSerial.print(F("\nUm Änderungen dauerhaft im Flash-Speicher zu sichern, muss das Konfigurationsmenü über die Option 7 beendet werden. Bei einem Neustart werden ungesicherte Änderungen verworfen.\n"));
    USBSerial.print(F("Auswahlmöglichkeiten zur Fehlersuche und Konfiguration des Fahrpults: \n\n"));  
    USBSerial.print(F("1 -> Wenn ein Taster/Schalter oder ein analoger Eingang betätigt wird, wird zusätzlich zu dem Datentelegramm der Pin und die I2C Adresse ausgegeben.\n"));
    USBSerial.print(F("2 -> Ausgabe der aktuellen Konfiguration im CSV style.\n"));
    USBSerial.print(F("3 -> Ändern der Port-Konfiguration oder der Konfiguration eines einzelnen Pins eines MCP-ICs. Es muss die I2C Addresse bekannt sein.\n"));
    USBSerial.print(F("4 -> Kanalnummer eines Ein-/ Ausgangs ändern. Es muss die I2C Addresse des dazugehörigen ICs und die Pin-Nummer bekannt sein.\n"));
    USBSerial.print(F("5 -> Anzahl der MCP oder PCF ICs ändern bzw. I2C Adressen (de-)aktivieren.\n"));
    if(VERBOSE){
        USBSerial.print(F("6 -> Debugging Ausgabe Ausschalten.\n"));
    }else{ 
        USBSerial.print(F("6 -> Debugging Ausgabe Einschalten.\n"));
    }
    USBSerial.print(F("7 -> Änderungen Speichern und Konfigurationsmenü beenden.\n"));
    USBSerial.print(F("8 -> Konfigurationsmenü beenden.\n"));
    USBSerial.print(F("9 -> Neustart. Nicht gesicherte Änderungen gehen verloren!\n"));
    USBSerial.print(F("10 -> Schnittstelle für die Kommunikation mit einem Simulator wählen.\n\n\n\n"));

    strcpy(info, "Diese Option steht nicht zur verfügung.\nGeben Sie eine der Verfügbaren Optionen ein, oder beenden Sie mit \"C\" das Konfigurationsmenü.\n");
    if (process_UserInput(buffer, &option, 2, 1, 11, info)) option = 8;   //exit menu, if user entered "C"
    
    USBSerial.printf("\nOption: %i\n", option);
    delete[] buffer;    //dead variable, not used 
    
    USBSerial.print("\n\n\n\n\n\n\n\n");
    USBSerial.print("-------------------------------------------------");
    USBSerial.print("-------------------------------------------------\n\n");

    switch (option)
    {
        case 1:
            opt_1();
            break;
        case 2:
            opt_2();
            break;
        case 3:
            opt_3();
            break;
        case 4:
            opt_4();
            break;
        case 5:
            opt_5();
            break;
        case 6: 
            opt_6();
            break;
        case 7:
            opt_7();
            break;
        case 8: 
            break;
        case 9: 
            ESP.restart();
            break;
        case 10: 
            opt_10();
            break;
        default:
            USBSerial.read();
            USBSerial.print(F("Bitte eine Ziffer von 0 bis 9 für die entsprechende Option eingeben. Erneut versuchen, oder Konfiguration beenden? (j/n)"));
            USBSerial.print(F("\n(Eine andere Eingabe als J wird als Nein interpretiert)\n"));
            while(USBSerial.available() < 1) {vTaskDelay(1);}
            if (strcmp(buffer, "j") == 0) config_menu = 1;
            break;
    }

    if (!config_menu) {
      USBSerial.print(F("\nKonfigurationsmenü beendet.\nUm in das Konfigurationsmenü zu gelangen bitte \"M\" eingeben."));
      USBSerial.print(F("\n\n-------------------------------------------------"));
      USBSerial.print(F("-------------------------------------------------\n\n\n\n\n\n\n\n"));
      esc();
    }
}


};