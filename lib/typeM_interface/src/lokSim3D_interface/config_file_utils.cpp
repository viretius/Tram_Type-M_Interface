/*
cvs files: 
  
  mcp:
  i2c;pin;kanal;io;key;adresse;info
     
  pcf: 
  i2c;pin;key;kanal;adresse;info

    -> address "tp" for combined throttle 
*/

#include "lokSim3D_interface/config_file_utils.h"

/*
*functions for file/data management
*openFile() functions return a File object. 
*This object supports all the functions of Stream, 
*so you can use readBytes, findUntil, parseInt, println, and all other Stream methods
*/ 

using namespace lokSim3D_interface;

namespace lokSim3D_config {

bool load_config()
{
  if(!LittleFS.begin(0, "/littlefs", 2U, "spiffs")) 
  {
    USBSerial.print(F("\nLittleFS Mount fehlgeschlagen.\nKonfiguration konnte nicht geladen werden.\nSetup abgebrochen.\n"));
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
  
  readFile(LittleFS, "/mcp.txt", mcp_buf, mcp_buf_size);
  readFile(LittleFS, "/pcf.txt", pcf_buf, pcf_buf_size);

  //=======================================================
  //parse data for mcp ICs
  //=======================================================

  //i2c;pin;kanal;io;key;adresse;info
  CSV_Parser mcp_parse(mcp_buf, "ucuc-ucsss" /*uc: unsigned char, s: string, -: unused*/, /*has header*/true, /*custom delimiter*/';');    
  
  USBSerial.println();
  mcp_parse.print();
  
  uint8_t *i2c = (uint8_t*)mcp_parse["i2c"];
  uint8_t *pin = (uint8_t*)mcp_parse["pin"];
  char **address = (char**)mcp_parse["addresse"]; //tcp: here used for outputs (leds and stuff)
  uint8_t *io = (uint8_t*)mcp_parse["io"];      //theoreticly not needed -> rows with key are inputs, rows with tcp-addresses are outputs
  char **key = (char**)mcp_parse["key"];        //used for inputs (buttons, switches, etc.), but also saved in mcp_list[].address[]
  char **info = (char**)mcp_parse["info"];      

  for (t = 0; t < mcp_parse.getRowsCount(); t++) 
  {
    if (info[t] != nullptr) 
    {
      mcp_list[i2c[t] - MCP_I2C_BASE_ADDRESS].info[pin[t]] = new char[strlen(info[t]) + 1];
      strcpy(mcp_list[i2c[t] - MCP_I2C_BASE_ADDRESS].info[pin[t]], info[t]);
    }
    if (i2c[t] < MCP_I2C_BASE_ADDRESS || i2c[t] > MCP_I2C_END_ADDRESS) //check if i2c address is in valid range
    {
      USBSerial.printf("I2C-Adresse liegt nicht zwischen %i und %i: %u\n", MCP_I2C_BASE_ADDRESS, MCP_I2C_END_ADDRESS, i2c[t]);
      strcpy(mcp_list[i2c[t] - MCP_I2C_BASE_ADDRESS].address[pin[t]], "-1");      
    }
    else if (pin[t] < 0 || pin[t] > 15) //
    {
      USBSerial.printf("Pin liegt nicht zwischen 0 und 15: %u\n", pin[t]);
      strcpy(mcp_list[i2c[t] - MCP_I2C_BASE_ADDRESS].address[pin[t]], "-1");      
    }
    else if (strcmp(address[t], "-1") == 0 || address[t] == nullptr) 
    {
      USBSerial.printf("Pin %u an IC mit Adresse %i (DEC) nicht in Verwendung.\n", pin[t], i2c[t]);
    }
    else {      
      //evalute if pin is input or output by checking if key is set or not instead of checking io
      //Store the key value in mcp_list's address array 
      if (key[t] != nullptr)
      {
        //pin is a input
        bitWrite(mcp_list[i2c[t] - MCP_I2C_BASE_ADDRESS].portMode, pin[t], 1); 

        // Allocate memory dynamically based on the length of the key value
        size_t keyLength = strlen(key[t]) + 1; // +1 for null-terminator
        mcp_list[i2c[t] - MCP_I2C_BASE_ADDRESS].address[pin[t]] = new char[keyLength];
        memset(mcp_list[i2c[t] - MCP_I2C_BASE_ADDRESS].address[pin[t]], '\0', keyLength); // Clear the memory

        // Copy the key value to the dynamically allocated memory
        strcpy(mcp_list[i2c[t] - MCP_I2C_BASE_ADDRESS].address[pin[t]], key[t]);
      }
      //output - save address value in mcp_list's address array
      else {
        //pin is a output
        bitWrite(mcp_list[i2c[t] - MCP_I2C_BASE_ADDRESS].portMode, pin[t], 0); 

        mcp_list[i2c[t] - MCP_I2C_BASE_ADDRESS].address[pin[t]] = new char[3]; //address konsists of 2 digits and null-terminator 
        memset(mcp_list[i2c[t] - MCP_I2C_BASE_ADDRESS].address[pin[t]], '\0', 3); // Clear the memory
        
        //custom buttons, status affects the data, that will be transmitted after change of the combined throttle
        //custom buttons, status affects the data, that will be transmitted after change of the combined throttle
       if (strncmp(address[t], "ac(", 3) == 0) //accereleration detection
       {
        acceleration_button[0] = i2c[t];
        acceleration_button[1] = pin[t];
    
        // extract values inside of the brackets
        char* start = strchr(address[t], '(');
        char* end = strchr(address[t], ')');
          
        if (start != nullptr && end != nullptr && start < end) 
        {
          *end = '\0'; //replace end bracket with null-terminator
          char* adr = start + 1; 
          acceleration_button[3] = static_cast<uint8_t>(atoi(adr)); //convert to int with atoi and cast to uint8_t
        } else {
              USBSerial.println("Fehler: Ungültiges Format für ac-Kanalnummer.");
            }
      } 
      else if (strncmp(address[t], "dc(", 3) == 0)
      {
        deceleration_button[0] = i2c[t];
        deceleration_button[1] = pin[t];
          
        char* start = strchr(address[t], '(');
        char* end = strchr(address[t], ')');
          
        if (start != nullptr && end != nullptr && start < end) 
        {
          *end = '\0'; // Ende der Zeichenkette nach der Zahl
          char* adr = start + 1; // Zeiger auf die Zahl nach '('
          deceleration_button[3] = static_cast<uint8_t>(atoi(adr));
        } else {
            USBSerial.println("Fehler: Ungültiges Format für dc-Kanalnummer.");
            }
      }
        else {    
            // Copy the key value to the dynamically allocated memory
            strcpy(mcp_list[i2c[t] - MCP_I2C_BASE_ADDRESS].address[pin[t]], address[t]);  
        }
      }

    }

    if ((t == mcp_parse.getRowsCount()-1) || (i2c[t] != i2c[t+1])) //end of file reached
    {  
      mcp_count++;
      mcp_list[i2c[t] - MCP_I2C_BASE_ADDRESS].enabled = true;
      if (t == mcp_parse.getRowsCount()-1) break;
    }
  } 

  USBSerial.printf("mcp_count: %i\n", mcp_count);

  //=======================================================
  //parse data for pcf ICs
  //=======================================================

  USBSerial.print(F("\nLese Konfigurationsdatei \"pcf.txt\"...\n"));
  
  //i2c;pin;kanal;key;adresse;info
  CSV_Parser pcf_parse(pcf_buf, "ucuc-sss" /*uc: unsigned char, s: string*/, /*has header*/true, /*custom delimiter*/';');
 
  USBSerial.println();
  pcf_parse.print();

  i2c = (uint8_t*)pcf_parse["i2c"];
  pin = (uint8_t*)pcf_parse["pin"];
  address = (char**)pcf_parse["addresse"];
  key = (char**)pcf_parse["key"];
  info = (char**)pcf_parse["info"];

  for (t = 0; t < pcf_parse.getRowsCount(); t++)
  {
    if (info[t] != nullptr) 
    {
      pcf_list[i2c[t] - PCF_I2C_BASE_ADDRESS].info[pin[t]] = new char[strlen(info[t]) + 1];
      strcpy(pcf_list[i2c[t] - PCF_I2C_BASE_ADDRESS].info[pin[t]], info[t]);
    }
    if (i2c[t] < PCF_I2C_BASE_ADDRESS || i2c[t] > PCF_I2C_END_ADDRESS) //check if i2c address is in valid range
    {
      USBSerial.printf("I2C-Adresse liegt nicht zwischen %i und %i: %u\n", PCF_I2C_BASE_ADDRESS, PCF_I2C_END_ADDRESS, i2c[t]);
      strcpy(mcp_list[i2c[t] - PCF_I2C_BASE_ADDRESS].address[pin[t]], "-1");      
    }
    if (pin[t] < 0 || pin[t] > 4) //
    {
      USBSerial.printf("Pin liegt nicht zwischen 0 und 4: %u\n", pin[t]);
      strcpy(pcf_list[i2c[t]-PCF_I2C_BASE_ADDRESS].address[pin[t]], "-1");   
    }
    else if (strcmp(address[t], "-1") == 0 || address[t] == nullptr) 
    {
      USBSerial.printf("Pin %u an IC mit Adresse %i (DEC) nicht in verwendung.\n", pin[t], i2c[t]);
    }
    else {
      if (pin[t] != 4) //input (pcf8591: pin 4: output, pins 0-3: input)
      {
        //Store the key value in pcf_list's address array: 
        
        size_t keyLength = strlen(key[t]) + 1; // +1 for null-terminator
        pcf_list[i2c[t] - PCF_I2C_BASE_ADDRESS].address[pin[t]] = new char[keyLength]; // Allocate memory dynamically based on the length of the key value
        memset(pcf_list[i2c[t] - PCF_I2C_BASE_ADDRESS].address[pin[t]], '\0', keyLength); // Clear the memory

        strcpy(pcf_list[i2c[t] - PCF_I2C_BASE_ADDRESS].address[pin[t]], key[t]);  // Copy the key value to the dynamically allocated memory
      }
      else {
        pcf_list[i2c[t] - PCF_I2C_BASE_ADDRESS].address[pin[t]] = new char[3]; //address konsists of 2 digits and null-terminator 
        memset(pcf_list[i2c[t] - PCF_I2C_BASE_ADDRESS].address[pin[t]], '\0', 3); // Clear the memory
        
        if (strncmp(address[t], "ct(", 3) == 0) combined_throttle_ic = i2c[t]; //address for accel / decel is stored in acceleration- / deceleration_button
        
        else {    
            //just Copy the key value to the dynamically allocated memory
            strcpy(pcf_list[i2c[t] - PCF_I2C_BASE_ADDRESS].address[pin[t]], address[t]);  
        }
      } 
    }

    if ((t == pcf_parse.getRowsCount() - 1) || (i2c[t] != i2c[t+1])) //end of file reached or next i2c address
    {
      pcf_count++;
      pcf_list[i2c[t] - PCF_I2C_BASE_ADDRESS].enabled = true;
      if (t == pcf_parse.getRowsCount() - 1) break;
    } 
  }
  
  USBSerial.printf("pcf_count: %i\n\n", pcf_count);

  //=======================================================
  //init ICs
  //=======================================================

  for (i = 0; i < MAX_IC_COUNT; i++) 
  {
    //=======================================================
    //MCP ICs
    //=======================================================

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
 
    //=======================================================
    //PCF ICs
    //=======================================================
    
    pcf_list[i].i2c = i + PCF_I2C_BASE_ADDRESS;
    pcf_list[i].pcf = Adafruit_PCF8591();

    if(!pcf_list[i].enabled) { //no entry in csv file for this i2c address found
      continue;
      //USBSerial.printf("pcf_list[%i].enabled: %i\n", i, pcf_list[i].enabled);
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

  //=======================================================
  //load network settings
  //=======================================================

  uint8_t ip[4] = {0, 0, 0, 0};
  preferences.begin("lokSim3D_net", true, "nvs");

    ip[0] = preferences.getUInt("ip1", 169);
    ip[1] = preferences.getUInt("ip2", 254);
    ip[2] = preferences.getUInt("ip3", 227);
    ip[3] = preferences.getUInt("ip4", 2);
    host = IPAddress(ip[0], ip[1], ip[2], ip[3]);

    port = preferences.getUInt("port", 1435);
    preferences.getBytes("mac", mac, 6);

  preferences.end();
  
  USBSerial.printf("\n\nPort: %u\n", port);
  USBSerial.printf("MAC-Adresse: %02X:%02X:%02X:%02X:%02X:%02X\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  USBSerial.printf("IP-Adresse: %u.%u.%u.%u\n", ip[0], ip[1], ip[2], ip[3]);
  


  return 1;
}

/*
==========================================================================================================================
==========================================================================================================================
*/
//==========================================================================================================================
//let user enter ip address
//==========================================================================================================================

static void process_IPInput() 
{
  char *buffer = new char[16]{'\0'}; //12 digits + 3 seperators(.) + null-terminator
  char info[250];        //info for user, if input didnt fit to request
  uint8_t ip[4] = {0, 0, 0, 0};
  uint8_t i = 0, t = 0;
  
  while (USBSerial.available()) USBSerial.read();
  USBSerial.print(F("\nGeben Sie jetzt die neue IPV4 Adresse im gängigen Format ein oder kehren sie mit \"C\" zurück zum Menü.\n"));

  //1: read input
  while(!USBSerial.available()) {vTaskDelay(1);} 
  if (USBSerial.peek() == 'C') return;     

  int bytesRead = 0;
  while (USBSerial.available()) 
  {
    bytesRead += USBSerial.readBytesUntil('\n', buffer + bytesRead, 16 - bytesRead); //max 16 bytes, but user could enter less. e.g. 192.168.4.1
    if (bytesRead > 0 || buffer[0] == '\n') break;
    vTaskDelay(1); 
  }
  buffer[bytesRead] = '\0'; 
  
  //2: validate input as ipv4 address:
  char *token = strtok(buffer, ".");
  while (token != NULL) 
  {
    ip[i] = atoi(token); //returs 0 if token is not a number
    //check if if token is a number
    if (ip[i] == 0 && token[0] != '0') 
    {
      USBSerial.print(F("\nUngültige Eingabe. Bitte geben Sie nur Zahlen und Punkte ein.\n"));
      delete[] buffer;
      process_IPInput();
      return;
    }
    //check boundaries
    if (ip[i] < 0 || ip[i] > 255) 
    {
      USBSerial.print(F("\nUngültige Eingabe. Bitte geben Sie Zahlen zwischen 0 und 255 ein.\n"));
      delete[] buffer;
      process_IPInput();
      return;
    }
    i++;
    token = strtok(NULL, ".");
  }
  if (i != 4) //check if 4 tuples have been entered
  {
    USBSerial.print(F("\nUngültige Eingabe. Bitte geben Sie 4 Zahlen ein.\n"));
    delete[] buffer;
    process_IPInput();
    return;
  }

  //3: ask user if input is correct
  USBSerial.printf("\nIhre Eingabe: %u.%u.%u.%u\n", ip[0], ip[1], ip[2], ip[3]);
  USBSerial.print(F("\nIst diese Eingabe korrekt? (j/n)"));

  while(!USBSerial.available()){vTaskDelay(1);}
  memset(&buffer[0], '\0', 3); 
  USBSerial.readBytesUntil('\n', buffer, 1); 
  USBSerial.printf("\n%s", buffer);
  if (strcmp(buffer, "j") == 0 || strcmp(buffer, "J") == 0)  
  {
    preferences.begin("lokSim3D_net", false);
    preferences.putUInt("ip1", ip[0]);
    preferences.putUInt("ip2", ip[1]);
    preferences.putUInt("ip3", ip[2]);
    preferences.putUInt("ip4", ip[3]);
    preferences.end();
    USBSerial.println(F("\nEinstellung gespeichert.\nMenü wird erneut aufgerufen..."));
  }
  else {
    delete[] buffer;
    process_IPInput();
  }
  delete[] buffer;

}


//==========================================================================================================================
//let user enter MAC address
//==========================================================================================================================

static void process_MACInput() 
{
  char *buffer = new char[12+5+1]{'\0'}; //12 digits + 5 seperators(:) + null-terminator
  char info[250];        //info for user, if input didnt fit to request
  byte _mac[6] = {0, 0, 0, 0, 0, 0};
  uint8_t i = 0, t = 0;

  while (USBSerial.available()) USBSerial.read();
  USBSerial.print(F("\nGeben Sie jetzt die neue MAC-Adresse im Format XX:XX:XX:XX:XX:XX ein oder kehren sie mit \"C\" zurück zum Menü.\n"));

  //1: read input
  while(!USBSerial.available()) {vTaskDelay(1);}
  if (USBSerial.peek() == 'C') return;
  USBSerial.readBytesUntil('\n', buffer, 18);

  //2: validate input as MAC address:
  char *token = strtok(buffer, ":");
  while(token != NULL) 
  {
    //check if if token is a hex number
    if (sscanf(token, "%hhx", &_mac[i]) != 1) 
    {
      USBSerial.print(F("\nUngültige Eingabe. Bitte geben Sie nur Hexadezimalzahlen und Doppelpunkte ein.\n"));
      delete[] buffer;
      process_MACInput(); 
      return;
    }
    i++;
    token = strtok(NULL, ":");
  }
  if (i != 6) //check if 4 tuples have been entered
  {
    USBSerial.print(F("\nUngültige Eingabe. Bitte geben Sie 6 Zahlen ein.\n"));
    delete[] buffer;
    process_MACInput(); 
    return; 
  }

  //3: ask user if input is correct

  USBSerial.printf("\nIhre Eingabe: %02X:%02X:%02X:%02X:%02X:%02X\n", _mac[0], _mac[1], _mac[2], _mac[3], _mac[4], _mac[5]);
  USBSerial.print(F("\nIst diese Eingabe korrekt? (j/n)"));

  while(!USBSerial.available()){vTaskDelay(1);}
  memset(&buffer[0], '\0', 3); 
  USBSerial.readBytesUntil('\n', buffer, 1); 
  USBSerial.printf("\n%s", buffer);
  if (strcmp(buffer, "j") == 0 || strcmp(buffer, "J") == 0) 
  {
    preferences.begin("lokSim3D_net", false);
    preferences.putBytes("mac", _mac, 6);
    preferences.end();
    USBSerial.println(F("\nEinstellung gespeichert.\nMenü wird erneut aufgerufen..."));
  }
  else {
    delete[] buffer;
    process_MACInput();
  }
  delete[] buffer;
}

//==========================================================================================================================
//let user change keyboard shortcut / tcp address of an input / output
//adjustments to be made for loksim3d ( )
//==========================================================================================================================
static void opt_4() {
                  
  uint8_t i, t;
  int i2c_adr;
  int pin;
  int new_address;
  char *buffer = new char[3]{'\0'};     //buffer for user-input 
  char current_address[3] = {'\0'};        
  char info[250] = {'\0'};               //info for user, if input didnt fit to request, static array, because there is enough memory, but memory allocation costs cpu cycles


  USBSerial.print(F("Sie können jederzeit zum Menü zurückgelangen. Geben Sie dazu ein \"C\" ein."));
  USBSerial.printf("\nGeben Sie die I2C-Addresse des ICs ein, dessen Port-Konfiguration Sie ändern möchten (%i-%i & %i-%i (DEC)): ", MCP_I2C_BASE_ADDRESS, MCP_I2C_END_ADDRESS, PCF_I2C_BASE_ADDRESS, PCF_I2C_END_ADDRESS);   
  strcpy(info, "\nDiese Adresse steht nicht zur verfügung.\nGeben Sie eine passende Adresse ein, oder kehren Sie mit \"C\" zurück zum Menü.\n");
  while (1)  //let user input some valid i2c adress or "C" to return back to the menu. if user entered a valid address, search the index in the lists where this adress is lokated at
  {  
    if (get_integer_with_range_check(&i2c_adr, MCP_I2C_BASE_ADDRESS, PCF_I2C_END_ADDRESS, info)) return;
    
    i = i2c_adr - MCP_I2C_BASE_ADDRESS; //assume, entered address belongs to an mcp ic, i = index in mcp_list 
    if (i < 9) {break;} else { i = i2c_adr - PCF_I2C_BASE_ADDRESS;} //yes? break while loop otherwhise check if it belongs to pcf ic (i = unsigned! cant be < 0)
    if (i < 9)  break; //yes? break, otherwhise go back to get_integer_with_range_check (user entered a number between 40 and 71)

    USBSerial.printf("%s", info);
  }

  USBSerial.print(F("\n\nGeben Sie den Pin ein, dessen Kanalnummer Sie ändern möchten."));
    
  if (i2c_adr < MCP_I2C_END_ADDRESS)  //mcp ic        //ask user, which pin he wants to change and then copy current address 
  {
    USBSerial.print(F("\nDabei entspricht eine Nummer von 0 bis 7 einem Pin an Port A. Höhere Nummern entsprechen einem Pin an Port B.\n Achtung! Es wird nicht überprüft, ob derselbe Kanal schon bei einem anderen I/O verwendet wird!\n")); 
    strcpy(info,  "\nDiesen Pin gibt es nicht\nGeben sie eine Nummer von 0 bis 15 für den Pin ein, den Sie ändern möchten oder kehren Sie mit \"C\" zurück zum Menü.\n");
    if (get_integer_with_range_check(&pin, 0, 15, info)) return; 

    strcpy(current_address, mcp_list[i].address[pin]);
    USBSerial.printf("\n\nAktuell ist für diesen I/O Kanal %s hinterlegt.\nGeben Sie jetzt eine neue Kanalnummer ein:", current_address);
    memset(&buffer[0], '\0', 3);
    strcpy(info,  "\nDiese Kanalnumer gibt es nicht\nGeben sie eine Nummer von 0 bis 99 ein oder kehren Sie mit \"C\" zurück zum Menü.\n");
    if (get_integer_with_range_check(&new_address, 0, 99, info)) return;

    itoa(new_address, buffer, 10); //convert int to string
    strncpy(mcp_list[i].address[pin], buffer, 2); //write new address to mcp list
    

  } else if (i2c_adr > PCF_I2C_BASE_ADDRESS) //pcf ic
  {
    USBSerial.print(F("\nDabei entspricht eine Nummer von 0 bis 3 einem der 4 analogen Eingänge. Eine 4 führt entsprechend zur Änderung des Kanals des analogen Ausgangs.\nAchtung! Es wird nicht überprüft, ob derselbe Kanal schon bei einem anderen I/O verwendet wird!\n"));
    memset(&buffer[0], '\0', 3);
    strcpy(info,  "\nDiesen Pin gibt es nicht\nGeben sie eine Nummer von 0 bis 4 für den Pin ein, den Sie ändern möchten oder kehren Sie mit \"C\" zurück zum Menü.\n");
    if (get_integer_with_range_check(&pin, 0, 4, info)) return;
    strcpy(current_address, pcf_list[i].address[pin]);

    USBSerial.printf("\n\nAktuell ist für diesen I/O Kanal %s hinterlegt.\nGeben Sie jetzt eine neue Kanalnummer ein.", current_address);
    memset(&buffer[0], '\0', 3);
    strcpy(info,  "\nDiese Kanalnumer gibt es nicht\nGeben sie eine Nummer von - bis 99 ein oder kehren Sie mit \"C\" zurück zum Menü.\n");
    if (get_integer_with_range_check(&new_address, -1, 99, info)) return;

    itoa(new_address, buffer, 10); //convert int to string
    strncpy(pcf_list[i].address[pin], buffer, 2);
  }
    
  USBSerial.print(F("\n\nKanalnummer erfolgreich aktualisiert.\n"));
 
  while (USBSerial.available()) {USBSerial.read();}
  USBSerial.print(F("\nMenü erneut aufrufen? (j/n)"));
  USBSerial.print(F("\n(Eine andere Eingabe als j wird als Nein interpretiert)\n"));
  while(!USBSerial.available()){vTaskDelay(1);}

  memset(&buffer[0], '\0', 3); 
  USBSerial.readBytesUntil('\n', buffer, 1); 
  USBSerial.printf("\n%s", buffer);
  if (strcmp(buffer, "j") == 0) run_config_task = 1;
  delete[] buffer;

}
//==========================================================================================================================
//Network settings
//==========================================================================================================================

static void opt_11() 
{
  int option = 0;
  char info[145];
  char *buffer = new char[3]{'\0'}; //two chars and null-terminator

  while (USBSerial.available() > 0) {USBSerial.read();}
  run_config_task = 1; 

  USBSerial.print(F("Was möchten Sie tun?\nAchtung! Änderungen an den Netzwerkeinstellungen können dazu führen, dass das Fahrpult nicht mehr erreichbar ist.\n Änderungen werden direkt gespeichert und sind nach einem Neustart aktiv.\n"));
  USBSerial.print(F("1 -> Server-IP anpassen\n2 -> MAC-Adresse anpassen\n3 -> Port anpassen\nC -> Zurück zum Menü\n"));

  strcpy(info, "Diese Option steht nicht zur verfügung.\nGeben Sie eine der Verfügbaren Optionen ein, oder beenden Sie mit \"C\" das Konfigurationsmenü.\n");
  if (get_integer_with_range_check(&option, 1, 3, info)) return;   //exit menu, if user entered "C"

  switch (option) 
  {
    case 1:
      USBSerial.printf("\nAktuell gespeicherte Server-IP: %s\n", host.toString());
      process_IPInput();
      break;
    case 2:
        USBSerial.printf("\nAktuelle MAC-Adresse: %02X:%02X:%02X:%02X:%02X:%02X\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        process_MACInput();
      break;
    case 3:
      USBSerial.printf("\nAktueller Port: %u", port);
      USBSerial.read();
      if (get_integer_with_range_check(&option, 0, INT16_MAX, info)) return;
      preferences.begin("APP", false, "nvs");
      preferences.putUInt("port", option);
      USBSerial.printf("\nPort erfolgreich geändert auf %i", preferences.getUInt("port", 0)); //read back to check if it was saved correctly
      preferences.end();
      break;
    default:
      USBSerial.read();
      USBSerial.print(F("Bitte eine Ziffer von 0 bis 9 für die entsprechende Option eingeben. Erneut versuchen, oder Konfiguration beenden? (j/n)"));
      USBSerial.print(F("\n(Eine andere Eingabe als J wird als Nein interpretiert)\n"));
      while(USBSerial.available() < 1) {vTaskDelay(1);}
      if (strcmp(buffer, "j") == 0) run_config_task = 1;
      break;
  }

  delete[] buffer; 

}

//==========================================================================================================================
//USBSerial Configuration Menu
//==========================================================================================================================

void serial_config_menu()
{

    int option = 0;
    char info[145];
    char *buffer = new char[3]{'\0'}; //two chars and null-terminator

    while (USBSerial.available() > 0) {USBSerial.read();}
     
    USBSerial.print(F("\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n"));
    USBSerial.print(F("\nUm Änderungen dauerhaft im Flash-Speicher zu sichern, muss das Konfigurationsmenü über die Option 7 beendet werden. Bei einem Neustart werden ungesicherte Änderungen verworfen.\n"));
    USBSerial.print(F("Auswahlmöglichkeiten zur Fehlersuche und Konfiguration des Fahrpults: \n\n"));  
    USBSerial.print(F("1 -> Wenn ein Taster/Schalter oder ein analoger Eingang betätigt wird, wird zusätzlich zu dem Datentelegramm der Pin und die I2C Adresse ausgegeben.\n"));
    USBSerial.print(F("2 -> Ausgabe der aktuellen Konfiguration im CSV style.\n"));
    USBSerial.print(F("3 -> Ändern der Port-Konfiguration oder der Konfiguration eines einzelnen Pins eines MCP-ICs. Es muss die I2C Addresse bekannt sein.\n"));
    USBSerial.print(F("4 -> Tastenkombination / TCP-Adresse eines Ein-/ Ausgangs ändern. Es muss die I2C Addresse des dazugehörigen ICs und die Pin-Nummer bekannt sein.\n"));
    USBSerial.print(F("5 -> Anzahl der MCP oder PCF ICs ändern bzw. I2C Adressen (de-)aktivieren.\n"));
    if(VERBOSE){
        USBSerial.print(F("6 -> Debugging Ausgabe Ausschalten.\n"));
    }else{ 
        USBSerial.print(F("6 -> Debugging Ausgabe Einschalten.\n"));
    }
    USBSerial.print(F("7 -> Änderungen Speichern und Konfigurationsmenü beenden.\n"));
    USBSerial.print(F("8 -> Konfigurationsmenü beenden.\n"));
    USBSerial.print(F("9 -> Neustart. Nicht gesicherte Änderungen (Ausgenommen Netzwerkkonfiguration) gehen verloren!\n"));
    USBSerial.print(F("10 -> Schnittstelle für die Kommunikation mit einem Simulator wählen.\n"));
    USBSerial.print(F("11 -> Netzwerkeinstellungen ändern\n"));

    strcpy(info, "Diese Option steht nicht zur verfügung.\nGeben Sie eine der Verfügbaren Optionen ein, oder beenden Sie mit \"C\" das Konfigurationsmenü.\n");
    if (get_integer_with_range_check(&option, 1, 11, info)) option = 8;   //exit menu, if user entered "C"
    
    delete[] buffer;    //dead variable, not used 
    
    USBSerial.print("\n\n");
    USBSerial.print("-------------------------------------------------");
    USBSerial.print("-------------------------------------------------\n\n\n");

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
            toggle_verbose();
            break;
        case 7:
            commit_config_to_fs();
            break;
        case 8: 
            break;
        case 9: 
            ESP.restart();
            break;
        case 10: 
            opt_10();
            break;
        case 11:
            opt_11();
            break;
        default:
            USBSerial.read();
            USBSerial.print(F("Bitte eine Ziffer von 0 bis 9 für die entsprechende Option eingeben. Erneut versuchen, oder Konfiguration beenden? (j/n)"));
            USBSerial.print(F("\n(Eine andere Eingabe als J wird als Nein interpretiert)\n"));
            while(USBSerial.available() < 1) {vTaskDelay(1);}
            if (strcmp(buffer, "j") == 0) run_config_task = 1;
            break;
    }

    if (!run_config_task) {
      USBSerial.print(F("\nKonfigurationsmenü beendet.\nUm in das Konfigurationsmenü zu gelangen bitte \"M\" eingeben."));
      USBSerial.print(F("\n\n-------------------------------------------------"));
      USBSerial.print(F("-------------------------------------------------\n\n\n\n\n\n\n\n"));
      //esc();
    }
}

}; //namespace ConfigMenu
