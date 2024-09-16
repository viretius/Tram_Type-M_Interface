#include <config_file_utils.h>

//=====================================================
//helper functions, definitions beginning at line 1105
//=====================================================
static bool get_integer_with_range_check (int *var, uint lower_bound, uint upper_bound, char *info);

static size_t getFilesize(fs::FS &fs, const char* path);

static void readFile(fs::FS &fs, const char *path, char *buffer, size_t len);

static void appendFile(fs::FS &fs, const char *path, const char *message);

static void clearFile(fs::FS &fs, const char *path);

static void choose_sim();

static void toggle_verbose();

//overwrite specific data depending on simulator //1: thmSim, 2: lokSim3D
//untested!!!
static void commit_config_to_fs(int sim);  
//helper functions for commit_config_to_fs
static void update_mcp_config(char* current_config, int sim, size_t config_size);
static void update_pcf_config(char* current_config, int sim, size_t config_size);



//***********************************************************************************************  */
//namespaces
//***********************************************************************************************  */
namespace thmSim_config {

bool load_config()
{
  if(!LittleFS.begin(0, "/littlefs", 2U, "spiffs")) 
  {
    Serial.print(F("\nLittleFS Mount fehlgeschlagen.\nKonfiguration konnte nicht geladen werden.\nSetup abgebrochen.\n"));
    return 0;
  }
  
  uint8_t i, t; //local for loop counter
  uint8_t mcp_count = 0, pcf_count = 0; //gets printed out-> user can see, how many ICs of one type will be initialized

  Serial.printf("\nDateigröße wird ermittelt: %s", "/mcp.txt");
  size_t mcp_buf_size = getFilesize(LittleFS, "/mcp.txt");

  Serial.printf("\nDateigröße wird ermittelt: %s\n", "/pcf.txt");
  size_t pcf_buf_size = getFilesize(LittleFS, "/pcf.txt");

  if (!mcp_buf_size || !pcf_buf_size) {
    return 0;
  }

  for (i = 0; i < MAX_IC_COUNT; i++) 
  {
    for (t = 0; t < 16; t++) 
    {
      mcp_list[i].address[t] = new char[3]{'\0'}; //address consists of 2 digits and null-terminator
    }
    for (t = 0; t < 5; t++) 
    {
      pcf_list[i].address[t] = new char[3]{'\0'}; 
    }
  }

  Serial.printf("\nmcp_buf_size: %i\npcf_buf_size: %i\n\n", mcp_buf_size, pcf_buf_size);

  char *mcp_buf = new char[mcp_buf_size]; 
  char *pcf_buf = new char[pcf_buf_size];
  
  readFile(LittleFS, "/mcp.txt", mcp_buf, mcp_buf_size);
  readFile(LittleFS, "/pcf.txt", pcf_buf, pcf_buf_size);

  //=================================================================================
  //parse data for mcp ICs and load 
  //=================================================================================

  //i2c;pin;kanal;io;key;adresse;info
  CSV_Parser mcp_parse(mcp_buf, "ucucsuc--s" /*uc: unsigned char, s: string, -: unused*/, /*has header*/true, /*custom delimiter*/';');    
  
  Serial.println();
  mcp_parse.print();
  
  uint8_t *i2c = (uint8_t*)mcp_parse["i2c"];
  uint8_t *pin = (uint8_t*)mcp_parse["pin"];
  char **address = (char**)mcp_parse["kanal"];
  uint8_t *io = (uint8_t*)mcp_parse["io"];      
  char **info = (char**)mcp_parse["info"];

  //loop through every row of the csv file
  for (t = 0; t < mcp_parse.getRowsCount(); t++) 
  {
    if (info[t] != nullptr) 
    {
      mcp_list[i2c[t] - MCP_I2C_BASE_ADDRESS].info[pin[t]] = new char[strlen(info[t]) + 1];
      strcpy(mcp_list[i2c[t] - MCP_I2C_BASE_ADDRESS].info[pin[t]], info[t]);
    }
    if (i2c[t] < MCP_I2C_BASE_ADDRESS || i2c[t] > MCP_I2C_END_ADDRESS) //check if i2c address is in valid range
    {
      Serial.printf("I2C-Adresse liegt nicht zwischen %i und %i: %u\n", MCP_I2C_BASE_ADDRESS, MCP_I2C_END_ADDRESS, i2c[t]);
      continue;
    }
    if (pin[t] < 0 || pin[t] > 15) 
    {
      Serial.printf("Pin liegt nicht zwischen 0 und 15: %u\n", pin[t]);
      continue;
    }
    
    bitWrite(mcp_list[i2c[t] - MCP_I2C_BASE_ADDRESS].portMode, pin[t], io[t]);

    //custom buttons, status affects the data, that will be transmitted after change of the combined-throttle
    bool is_ac = strncmp(address[t], "ac(", 3) == 0;
    bool is_dc = strncmp(address[t], "dc(", 3) == 0;

    if (is_ac || is_dc )            
    {
      if (is_ac) {       
        acceleration_button[0] = i2c[t];
        acceleration_button[1] = pin[t];
      }
      if (is_dc) {
        deceleration_button[0] = i2c[t];
        deceleration_button[1] = pin[t];
      }
      // extract values inside of the brackets
      char* start = strchr(address[t], '(');
      char* end = strchr(address[t], ')');
          
      if (start != nullptr && end != nullptr && start < end) 
      {
        *end = '\0';        //replace end bracket with null-terminator
        char* adr = start + 1; 
        strcpy( mcp_list[i2c[t] - MCP_I2C_BASE_ADDRESS].address[pin[t]], adr ); //copy extracted address to mcp_list
      } else {
          Serial.println("Fehler: Ungültiges Format für ac / dc Kanalnummer.");
        }
    } 
    else {   //"normal" output/input 
        //just Copy the address
        if (isdigit(address[t][0]) && isdigit(address[t][1])) { //is address a number?  
        strcpy(mcp_list[i2c[t] - MCP_I2C_BASE_ADDRESS].address[pin[t]], address[t]);  
        } else {
          strcpy(mcp_list[i2c[t] - MCP_I2C_BASE_ADDRESS].address[pin[t]], "-1");
        }
    }

    if ((t == mcp_parse.getRowsCount()-1) || (i2c[t] != i2c[t+1])) //end of file reached
    {  
      mcp_count++;
      mcp_list[i2c[t] - MCP_I2C_BASE_ADDRESS].enabled = true;
      if (t == mcp_parse.getRowsCount()-1) break;
    }
  } 

  Serial.printf("mcp_count: %i\n", mcp_count);

  //==========================================================================
  //parse data for pcf ICs
  //==========================================================================

  Serial.print(F("\nLese Konfigurationsdatei \"pcf.txt\"...\n"));
  
  //i2c;pin;kanal;key;adresse;info
  CSV_Parser pcf_parse(pcf_buf, "ucucs--s" /*uc: unsigned char, s: string*/, /*has header*/true, /*custom delimiter*/';');
 
  Serial.println();
  pcf_parse.print();

  i2c = (uint8_t*)pcf_parse["i2c"];
  pin = (uint8_t*)pcf_parse["pin"];
  address = (char**)pcf_parse["kanal"];
  info = (char**)mcp_parse["info"];

  for (t = 0; t < pcf_parse.getRowsCount(); t++)
  {
    if (info[t] != nullptr) 
    {
      pcf_list[i2c[t] - PCF_I2C_BASE_ADDRESS].info[pin[t]] = new char[strlen(info[t]) + 1];
      strcpy(pcf_list[i2c[t] - PCF_I2C_BASE_ADDRESS].info[pin[t]], info[t]);
    }
    if (i2c[t] < PCF_I2C_BASE_ADDRESS || i2c[t] > PCF_I2C_END_ADDRESS) //check if i2c address is in valid range
    {
      Serial.printf("I2C-Adresse liegt nicht zwischen %i und %i: %u\n", PCF_I2C_BASE_ADDRESS, PCF_I2C_END_ADDRESS, i2c[t]);
      continue;        
    }
    if (pin[t] < 0 || pin[t] > 4) //
    {
      Serial.printf("Pin liegt nicht zwischen 0 und 4: %u\n", pin[t]);
      continue;     
    } 
    
    if (strncmp(address[t], "ct", 3) == 0)
    {
      combined_throttle_ic[0] = i2c[t]; 
      combined_throttle_ic[1] = pin[t];
      //print out the address of the combined throttle, at this point, these are already set
      Serial.printf("Kombihebel gefunden. Kanalnummern: %i, %i", combined_throttle_ic[0], combined_throttle_ic[1]);
    } 
    else {    
        strcpy(pcf_list[i2c[t] - PCF_I2C_BASE_ADDRESS].address[pin[t]], address[t]);  
      }
    
    if ((t == pcf_parse.getRowsCount() - 1) || (i2c[t] != i2c[t+1])) //end of file reached or next i2c address
    {
      pcf_count++;
      pcf_list[i2c[t] - PCF_I2C_BASE_ADDRESS].enabled = true;
      if (t == pcf_parse.getRowsCount() - 1) break;
    } 
  }
  
  Serial.printf("pcf_count: %i\n\n", pcf_count);

  //==========================================================================
  //init ICs
  //==========================================================================

  for (i = 0; i < MAX_IC_COUNT; i++) 
  {
    
    //=====================================
    //MCP ICs
    //=====================================

    mcp_list[i].i2c = i + MCP_I2C_BASE_ADDRESS;
    mcp_list[i].mcp = MCP23017();

    if(!mcp_list[i].enabled) { //no entry in csv file for this i2c address found
      continue;
    }
      
    if (!mcp_list[i].mcp.init(mcp_list[i].i2c)) {
      Serial.printf("\nMCP23017 mit der Adresse 0x%i konnte nicht initialisiert werden.", mcp_list[i].i2c);
      mcp_list[i].enabled = false;
    }

    mcp_list[i].mcp.portMode(MCP23017Port::A, mcp_list[i].portMode & 0xFF); //input_pullups enabled by default, portA: LSB
    mcp_list[i].mcp.portMode(MCP23017Port::B, (mcp_list[i].portMode >> 8) & 0xFF); //portB: MSB
    mcp_list[i].mcp.writeRegister(MCP23017Register::GPIO_A, 0x00);
    mcp_list[i].mcp.writeRegister(MCP23017Register::GPIO_B, 0x00);  //Reset ports
    mcp_list[i].mcp.writeRegister(MCP23017Register::IPOL_A, 0x00);    
    mcp_list[i].mcp.writeRegister(MCP23017Register::IPOL_B, 0x00);  //If a bit is set, the corresponding GPIO register bit will reflect the inverted value on the pin                                                               
  
    mcp_list[i].last_reading = mcp_list[i].mcp.read(); //read the current state of the pins
    
    //==============================================================
    //PCF ICs
    //==============================================================

    pcf_list[i].i2c = i + PCF_I2C_BASE_ADDRESS;
    pcf_list[i].pcf = Adafruit_PCF8591();

    if(!pcf_list[i].enabled) { //no entry in csv file for this i2c address found
      continue;
    }
   
    if (!pcf_list[i].pcf.begin(pcf_list[i].i2c)) {
      Serial.printf("\n\nPCF8591 mit der Adresse 0x%x konnte nicht initialisiert werden.", pcf_list[i].i2c);
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

//====================================================================
//change address of a singel IO (I2C address & pin number needed)
//==================================================================

static void opt_1() 
{
  uint8_t i, t; //local for loop counter
  int i2c_adr, pin;  //user input
  int new_address; //user input, will be converted to string

  char buffer[3] = {'\0'};      //buffer for user-input
  char current_address[3] = {'\0'};        
  char info[250] = {'\0'}; //info for user, if input didnt fit to request


  Serial.print(F("Sie können jederzeit zum Menü zurückgelangen. Geben Sie dazu ein \"C\" ein."));
  Serial.printf("\nGeben Sie die I2C-Addresse des ICs ein, dessen Port-Konfiguration Sie ändern möchten (%i-%i & %i-%i (DEC)): ", MCP_I2C_BASE_ADDRESS, MCP_I2C_END_ADDRESS, PCF_I2C_BASE_ADDRESS, PCF_I2C_END_ADDRESS);   
  
  strcpy(info, "\nDiese Adresse steht nicht zur verfügung.\nGeben Sie eine passende Adresse ein, oder kehren Sie mit \"C\" zurück zum Menü.\n");
  while (1)  //let user input some valid i2c adress or "C" to return back to the menu. if user entered a valid address, search the index in the lists where this adress is lokated at
  {  
    if (get_integer_with_range_check(&i2c_adr, MCP_I2C_BASE_ADDRESS, PCF_I2C_END_ADDRESS, info)) return; //user entered "C" -> return to menu
    
    //assume, entered i2c_address belongs to an mcp ic by checking the difference between the entered address and the base address of mcp-ICs
    i = i2c_adr - MCP_I2C_BASE_ADDRESS; 
    //is the assumption correct? -> break while loop, otherwhise check if it belongs to pcf-ICs (i = unsigned! cant be < 0)
    if (i < 9) {break;} else { i = i2c_adr - PCF_I2C_BASE_ADDRESS;} 
    //does it belong to the pcf-ICs? -> break 
    if (i < 9)  break; 
    //otherwhise (user entered a number between 40 and 71) go back to get_integer_with_range_check 
    Serial.printf("%s", info);
  }

  Serial.print(F("\n\nGeben Sie den Pin ein, dessen Kanalnummer Sie ändern möchten."));
    
  if (i2c_adr < MCP_I2C_END_ADDRESS)  //mcp ic        //ask user, which pin he wants to change and then copy current address 
  {
    Serial.print(F("\nDabei entspricht eine Nummer von 0 bis 7 einem Pin an Port A. Höhere Nummern entsprechen einem Pin an Port B.\n Achtung! Es wird nicht überprüft, ob derselbe Kanal schon bei einem anderen I/O verwendet wird!\n")); 
    strcpy(info,  "\nDiesen Pin gibt es nicht\nGeben sie eine Nummer von 0 bis 15 für den Pin ein, den Sie ändern möchten oder kehren Sie mit \"C\" zurück zum Menü.\n");
    if (get_integer_with_range_check(&pin, 0, 15, info)) return; 

    strcpy(current_address, mcp_list[i].address[pin]);
    Serial.printf("\n\nAktuell ist für diesen I/O Kanal %s hinterlegt.\nGeben Sie jetzt eine neue Kanalnummer ein:", current_address);
    memset(&buffer[0], '\0', 3);
    strcpy(info,  "\nDiese Kanalnumer gibt es nicht\nGeben sie eine Nummer von 0 bis 99 ein oder kehren Sie mit \"C\" zurück zum Menü.\n");
    if (get_integer_with_range_check(&new_address, 0, 99, info)) return;

    itoa(new_address, buffer, 10); //convert int to string
    strncpy(mcp_list[i].address[pin], buffer, 2); //write new address to mcp list
    

  } else if (i2c_adr > PCF_I2C_BASE_ADDRESS) //pcf ic
  {
    Serial.print(F("\nDabei entspricht eine Nummer von 0 bis 3 einem der 4 analogen Eingänge. Eine 4 führt entsprechend zur Änderung des Kanals des analogen Ausgangs.\nAchtung! Es wird nicht überprüft, ob derselbe Kanal schon bei einem anderen I/O verwendet wird!\n"));
    memset(&buffer[0], '\0', 3);
    strcpy(info,  "\nDiesen Pin gibt es nicht\nGeben sie eine Nummer von 0 bis 4 für den Pin ein, den Sie ändern möchten oder kehren Sie mit \"C\" zurück zum Menü.\n");
    if (get_integer_with_range_check(&pin, 0, 4, info)) return;
    strcpy(current_address, pcf_list[i].address[pin]);

    Serial.printf("\n\nAktuell ist für diesen I/O Kanal %s hinterlegt.\nGeben Sie jetzt eine neue Kanalnummer ein.", current_address);
    memset(&buffer[0], '\0', 3);
    strcpy(info,  "\nDiese Kanalnumer gibt es nicht\nGeben sie eine Nummer von - bis 99 ein oder kehren Sie mit \"C\" zurück zum Menü.\n");
    if (get_integer_with_range_check(&new_address, -1, 99, info)) return;

    itoa(new_address, buffer, 10); //convert int to string
    strncpy(pcf_list[i].address[pin], buffer, 2);
  }
    
  Serial.print(F("\n\nKanalnummer erfolgreich aktualisiert.\n"));
  memset(&buffer[0], '\0', 3); 

  while (Serial.available()) {Serial.read();} //clear serial buffer
  Serial.print(F("\nUm das Menü erneut aufzurufen, geben Sie \"j\" ein. Andernfalls wird das Menü beendet.\n"));
  while(!Serial.available()){vTaskDelay(1);} //wait for user input

  sprintf(buffer, "%s", Serial.read()); //read one char from serial buffer
  Serial.printf("\n%s", buffer);        //print the char for confirmation, that something was read
  if (strcmp(buffer, "j") == 0 ) run_config_task = 1;
}

//==============================
//Serial Configuration Menu
//=============================

void serial_config_menu()
{

    int option = 0; //user input
    char info[145];

    while (Serial.available() > 0) {Serial.read();}
    
    //prevent the menu from running again, if user didnt explicitly choose to do so
    run_config_task = 0; 
     
    Serial.print(F("\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n"));
    Serial.print(F("\nUm Änderungen dauerhaft im Flash-Speicher zu sichern, muss das Konfigurationsmenü über die Option 7 beendet werden. Bei einem Neustart werden ungesicherte Änderungen verworfen.\n"));
    Serial.print(F("Auswahlmöglichkeiten zur Fehlersuche und Konfiguration des Fahrpults: \n\n"));  
    Serial.print(F("1 -> Kanalnummer eines Ein-/ Ausgangs ändern. Dafür müssen I2C-Address und Pin bekannt sein.\n")); 
    if(VERBOSE){
        Serial.print(F("2 -> Debugging Ausgabe Ausschalten.\n"));
    }else{ 
        Serial.print(F("2 -> Debugging Ausgabe Einschalten.\n"));
    }
    Serial.print(F("3 -> Änderungen Speichern und Konfigurationsmenü beenden.\n"));
    Serial.print(F("4 -> Konfigurationsmenü beenden.\n"));
    Serial.print(F("5 -> Neustart. Nicht gesicherte Änderungen gehen verloren!\n"));
    Serial.print(F("6 -> Schnittstelle für die Kommunikation mit einem Simulator konfigurieren.\n\n"));

    strcpy(info, "Diese Option steht nicht zur verfügung.\nGeben Sie eine der Verfügbaren Optionen ein, oder beenden Sie mit \"C\" das Konfigurationsmenü.\n");
    if (get_integer_with_range_check(&option, 1, 6, info)) option = 4;   //exit menu, if user entered "C"
    
    Serial.print("\n\n");
    Serial.print("-------------------------------------------------");
    Serial.print("-------------------------------------------------\n\n\n");

    switch (option)
    {
        case 1:
            opt_1();
            break;
        case 2:
            toggle_verbose();
            break;
        case 3:
            Serial.println("FUNKTION IST UNGETESTET. NUTZUNG AUF EIGENE GEFAHR!");
            vTaskDelay(pdMS_TO_TICKS(1000));
            commit_config_to_fs(1);
            break;
        case 4://do nothing to escape the menu
            break;
        case 5:
            ESP.restart();
            break;
        case 6: 
            choose_sim();
            break;
        default:
            Serial.read();
            Serial.print(F("Bitte eine Ziffer von 0 bis 6 für die entsprechende Option eingeben."));
            Serial.print(F("\nUm das Menü erneut aufzurufen, geben Sie \"j\" ein. Andernfalls wird das Menü beendet.\n"));
            
            while(Serial.available() < 1) {vTaskDelay(1);}
            char buffer[2]; 
            memset(&buffer[0], '\0', 2); 
            sprintf(buffer, "%s", Serial.read());
            Serial.printf("\n%s", buffer);
            if (strcmp(buffer, "j") == 0) run_config_task = 1;
            break;
    }

    if (!run_config_task) { 
      Serial.print(F("\nKonfigurationsmenü beendet.\nUm in das Konfigurationsmenü zu gelangen bitte \"M\" eingeben."));
      Serial.print(F("\n\n-------------------------------------------------"));
      Serial.print(F("-------------------------------------------------\n\n\n\n\n\n\n\n"));
    }
}

}; //end of namespace thmSim_config

//***********************************************************************************************  */
//***********************************************************************************************  */
#if ( ARDUINO_NANO_ESP32)

namespace lokSim3D_config {

bool load_config()
{
  if(!LittleFS.begin(0, "/littlefs", 2U, "spiffs")) 
  {
    Serial.print(F("\nLittleFS Mount fehlgeschlagen.\nKonfiguration konnte nicht geladen werden.\nSetup abgebrochen.\n"));
    return 0;
  }
  
  uint8_t i, t; //local for loop counter
  uint8_t mcp_count = 0, pcf_count = 0; //gets printed out-> user can see, how many ICs of one type will be initialized

  Serial.printf("\nDateigröße wird ermittelt: %s", "/mcp.txt");
  size_t mcp_buf_size = getFilesize(LittleFS, "/mcp.txt");

  Serial.printf("\nDateigröße wird ermittelt: %s\n", "/pcf.txt");
  size_t pcf_buf_size = getFilesize(LittleFS, "/pcf.txt");

  if (!mcp_buf_size || !pcf_buf_size) {
    return 0;
  }

  Serial.printf("\nmcp_buf_size: %i\npcf_buf_size: %i\n\n", mcp_buf_size, pcf_buf_size);

  char *mcp_buf = new char[mcp_buf_size]; 
  char *pcf_buf = new char[pcf_buf_size];
  
  readFile(LittleFS, "/mcp.txt", mcp_buf, mcp_buf_size);
  readFile(LittleFS, "/pcf.txt", pcf_buf, pcf_buf_size);

  //=======================================================
  //parse data for mcp ICs
  //=======================================================

  //i2c;pin;kanal;io;key;adresse;info
  CSV_Parser mcp_parse(mcp_buf, "ucuc--sss" /*uc: unsigned char, s: string, -: unused*/, /*has header*/true, /*custom delimiter*/';');    
  
  Serial.println();
  mcp_parse.print();
  
  uint8_t *i2c = (uint8_t*)mcp_parse["i2c"];
  uint8_t *pin = (uint8_t*)mcp_parse["pin"];
  char **address = (char**)mcp_parse["adresse"]; //tcp: here used for outputs (leds and stuff)
  //uint8_t *io = (uint8_t*)mcp_parse["io"];      //theoreticly not needed -> rows with key are inputs, rows with tcp-addresses are outputs
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
      Serial.printf("I2C-Adresse liegt nicht zwischen %i und %i: %u\n", MCP_I2C_BASE_ADDRESS, MCP_I2C_END_ADDRESS, i2c[t]);
      continue;
    }
    if (pin[t] < 0 || pin[t] > 15) //
    {
      Serial.printf("Pin liegt nicht zwischen 0 und 15: %u\n", pin[t]);
      continue;      
    } 
        
    //evalute if pin is input or output by checking if key is set or not instead of checking io
    //Store the key value in mcp_list's address array 
    if (strlen(key[t]) >= 1) //no key value -> output
    {
      //pin is a input
      bitWrite(mcp_list[i2c[t] - MCP_I2C_BASE_ADDRESS].portMode, pin[t], 1); 
        
      bool is_ac = strncmp(address[t], "ac(", 3) == 0;
      bool is_dc = strncmp(address[t], "dc(", 3) == 0;
      //custom buttons, status affects the data, that will be transmitted after change of the combined throttle
      if (is_ac || is_dc ) //accereleration detection
      {
        if (is_ac) {       
          acceleration_button[0] = i2c[t];
          acceleration_button[1] = pin[t];
        }
        if (is_dc) {
          deceleration_button[0] = i2c[t];
          deceleration_button[1] = pin[t];
        }

        // extract values inside of the brackets
        char* start = strchr(address[t], '(');
        char* end = strchr(address[t], ')');
          
        if (start != nullptr && end != nullptr && start < end) 
        {   
          *end = '\0'; //replace end bracket with null-terminator
          char* _key = start + 1; //start with the character after the opening bracket
          size_t keyLength = strlen(_key) + 1;
          //Serial.printf("found special intput. key: %s\n", _key); //debug

          mcp_list[i2c[t] - MCP_I2C_BASE_ADDRESS].address[pin[t]] = new char[keyLength];
          memset(mcp_list[i2c[t] - MCP_I2C_BASE_ADDRESS].address[pin[t]], '\0', keyLength); // Clear the memory
          strcpy(mcp_list[i2c[t] - MCP_I2C_BASE_ADDRESS].address[pin[t]], _key);  
        } else {
            Serial.println("Fehler: Ungültiges Format für ac-Kanalnummer.");
        }
      } 
      else {   //"normal" input
          // Copy the key value to the dynamically allocated memory
          // Allocate memory dynamically based on the length of the key value
          size_t keyLength = strlen(key[t]) + 1; // +1 for null-terminator
          mcp_list[i2c[t] - MCP_I2C_BASE_ADDRESS].address[pin[t]] = new char[keyLength];
          memset(mcp_list[i2c[t] - MCP_I2C_BASE_ADDRESS].address[pin[t]], '\0', keyLength); // Clear the memory
          strcpy(mcp_list[i2c[t] - MCP_I2C_BASE_ADDRESS].address[pin[t]], key[t]);
        }
    }
    //output
    else 
    {
      bitWrite(mcp_list[i2c[t] - MCP_I2C_BASE_ADDRESS].portMode, pin[t], 0); 

      size_t len = strlen(address[t]+1);
      mcp_list[i2c[t] - MCP_I2C_BASE_ADDRESS].address[pin[t]] = new char[len]; //address consists of 2 digits and null-terminator 
      memset(mcp_list[i2c[t] - MCP_I2C_BASE_ADDRESS].address[pin[t]], '\0', len); // Clear the memory
        
      if (isdigit(address[t][0]) && isdigit(address[t][1]))
      { // Copy the key value to the dynamically allocated memory
        strcpy(mcp_list[i2c[t] - MCP_I2C_BASE_ADDRESS].address[pin[t]], address[t]);  
      } else {
        strncpy(mcp_list[i2c[t] - MCP_I2C_BASE_ADDRESS].address[pin[t]], "-1", 2);  
      }
    }
  

    if ((t == mcp_parse.getRowsCount()-1) || (i2c[t] != i2c[t+1])) //end of file reached
    {  
      mcp_count++;
      mcp_list[i2c[t] - MCP_I2C_BASE_ADDRESS].enabled = true;
      if (t == mcp_parse.getRowsCount()-1) break;
    }
  }

  Serial.printf("mcp_count: %i\n", mcp_count);

  //=======================================================
  //parse data for pcf ICs
  //=======================================================

  Serial.print(F("\nLese Konfigurationsdatei \"pcf.txt\"...\n"));
  
  //i2c;pin;kanal;key;adresse;info
  CSV_Parser pcf_parse(pcf_buf, "ucuc-sss" /*uc: unsigned char, s: string*/, /*has header*/true, /*custom delimiter*/';');
 
  Serial.println();
  pcf_parse.print();

  i2c = (uint8_t*)pcf_parse["i2c"];
  pin = (uint8_t*)pcf_parse["pin"];
  address = (char**)pcf_parse["adresse"];
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
      Serial.printf("I2C-Adresse liegt nicht zwischen %i und %i: %u\n", PCF_I2C_BASE_ADDRESS, PCF_I2C_END_ADDRESS, i2c[t]);
      continue;      
    }
    if (pin[t] < 0 || pin[t] > 4) //
    {
      Serial.printf("Pin liegt nicht zwischen 0 und 4: %u\n", pin[t]);
      continue;     
    }
    {
      if (pin[t] != 4) //input (pcf8591: pin 4: output, pins 0-3: input)
      {
        //Store the key value in pcf_list's address array: 
        
        size_t keyLength = strlen(key[t]) + 1; // +1 for null-terminator
        pcf_list[i2c[t] - PCF_I2C_BASE_ADDRESS].address[pin[t]] = new char[keyLength]; // Allocate memory dynamically based on the length of the key value
        memset(pcf_list[i2c[t] - PCF_I2C_BASE_ADDRESS].address[pin[t]], '\0', keyLength); // Clear the memory

        strcpy(pcf_list[i2c[t] - PCF_I2C_BASE_ADDRESS].address[pin[t]], key[t]);  // Copy the key value to the dynamically allocated memory
      }
      else 
      { //
        size_t len = strlen(address[t]+1);
        pcf_list[i2c[t] - PCF_I2C_BASE_ADDRESS].address[pin[t]] = new char[len]; //address consists of 2 digits and null-terminator 
        memset(pcf_list[i2c[t] - PCF_I2C_BASE_ADDRESS].address[pin[t]], '\0', len); // Clear the memory
        
        if (strncmp(address[t], "ct", 3) == 0) 
        {
          combined_throttle_ic[0] = i2c[t]; //address for accel / decel is stored in acceleration- / deceleration_button
          combined_throttle_ic[1] = pin[t];
        }

        //just Copy the key value to the dynamically allocated memory
        strcpy(pcf_list[i2c[t] - PCF_I2C_BASE_ADDRESS].address[pin[t]], address[t]);  
      } 
    }

    if ((t == pcf_parse.getRowsCount() - 1) || (i2c[t] != i2c[t+1])) //end of file reached or next i2c address
    {
      pcf_count++;
      pcf_list[i2c[t] - PCF_I2C_BASE_ADDRESS].enabled = true;
      if (t == pcf_parse.getRowsCount() - 1) break;
    } 
  }
  
  Serial.printf("pcf_count: %i\n\n", pcf_count);

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

    if(!mcp_list[i].enabled) { //no entry in csv file for this i2c address found
      continue;
    }
      
    if (!mcp_list[i].mcp.init(mcp_list[i].i2c)) {
      Serial.printf("\nMCP23017 mit der Adresse 0x%i konnte nicht initialisiert werden.", mcp_list[i].i2c);
      mcp_list[i].enabled = false;
    }

    mcp_list[i].mcp.portMode(MCP23017Port::A, mcp_list[i].portMode & 0xFF); //input_pullups enabled by default, portA: LSB
    mcp_list[i].mcp.portMode(MCP23017Port::B, (mcp_list[i].portMode >> 8) & 0xFF); //portB: MSB
    mcp_list[i].mcp.writeRegister(MCP23017Register::GPIO_A, 0x00);
    mcp_list[i].mcp.writeRegister(MCP23017Register::GPIO_B, 0x00);  //Reset ports
    mcp_list[i].mcp.writeRegister(MCP23017Register::IPOL_A, 0x00);    
    mcp_list[i].mcp.writeRegister(MCP23017Register::IPOL_B, 0x00);  //If a bit is set, the corresponding GPIO register bit will reflect the inverted value on the pin                                                               
 
    mcp_list[i].last_reading = mcp_list[i].mcp.read(); //read the current state of the pins
    //=======================================================
    //PCF ICs
    //=======================================================
    
    pcf_list[i].i2c = i + PCF_I2C_BASE_ADDRESS;
    pcf_list[i].pcf = Adafruit_PCF8591();

    if(!pcf_list[i].enabled) { //no entry in csv file for this i2c address found
      continue;
      //Serial.printf("pcf_list[%i].enabled: %i\n", i, pcf_list[i].enabled);
    }
   
    if (!pcf_list[i].pcf.begin(pcf_list[i].i2c)) {
      Serial.printf("\n\nPCF8591 mit der Adresse 0x%x konnte nicht initialisiert werden.", pcf_list[i].i2c);
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
  
  Serial.printf("\n\nPort: %u\n", port);
  Serial.printf("MAC-Adresse: %02X:%02X:%02X:%02X:%02X:%02X\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  Serial.printf("IP-Adresse: %u.%u.%u.%u\n", ip[0], ip[1], ip[2], ip[3]);
  


  return 1;
}

//=============================
//let user enter ip address
//==============================

static void process_IPInput() 
{
  char *buffer = new char[16]{'\0'}; //12 digits + 3 seperators(.) + null-terminator
  char info[250];        //info for user, if input didnt fit to request
  uint8_t ip[4] = {0, 0, 0, 0};
  uint8_t i = 0, t = 0;
  
  while (Serial.available()) Serial.read();
  Serial.print(F("\nGeben Sie jetzt die neue IPV4 Adresse im gängigen Format ein oder kehren sie mit \"C\" zurück zum Menü.\n"));

  //1: read input 
  while(!Serial.available()) {vTaskDelay(1);} 
  if (Serial.peek() == 'C') return;     //return to menu if user enters "C"

  int bytesRead = 0;
  while (Serial.available()) 
  {
    bytesRead += Serial.readBytesUntil('\n', buffer + bytesRead, 16 - bytesRead); //max 16 bytes, but user could enter less. e.g. 192.168.4.1
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
      Serial.print(F("\nUngültige Eingabe. Bitte geben Sie nur Zahlen und Punkte ein.\n"));
      delete[] buffer;
      process_IPInput();
      return;
    }
    //check boundaries
    if (ip[i] < 0 || ip[i] > 255) 
    {
      Serial.print(F("\nUngültige Eingabe. Bitte geben Sie Zahlen zwischen 0 und 255 ein.\n"));
      delete[] buffer;
      process_IPInput();
      return;
    }
    i++;
    token = strtok(NULL, ".");
  }
  if (i != 4) //check if 4 tuples have been entered
  {
    Serial.print(F("\nUngültige Eingabe. Bitte geben Sie 4 Zahlen ein.\n"));
    delete[] buffer;
    process_IPInput();
    return;
  }

  //3: ask user if input is correct
  Serial.printf("\nIhre Eingabe: %u.%u.%u.%u\n", ip[0], ip[1], ip[2], ip[3]);
  Serial.print(F("\nIst diese Eingabe korrekt? (j/n)"));

  while(!Serial.available()){vTaskDelay(1);}
  memset(&buffer[0], '\0', 3); 
  Serial.readBytesUntil('\n', buffer, 1); 
  Serial.printf("\n%s", buffer);
  if (strcmp(buffer, "j") == 0 || strcmp(buffer, "J") == 0)  //user confirmed input -> save to preferences
  {
    preferences.begin("lokSim3D_net", false);
    preferences.putUInt("ip1", ip[0]);
    preferences.putUInt("ip2", ip[1]);
    preferences.putUInt("ip3", ip[2]);
    preferences.putUInt("ip4", ip[3]);
    preferences.end();
    Serial.println(F("\nEinstellung gespeichert.\nMenü wird erneut aufgerufen..."));
  }
  else {
    delete[] buffer;
    process_IPInput(); //user denied input -> repeat, user can enter "c" to return to menu
  }
  delete[] buffer;

}


//===========================
//let user enter MAC address
//===========================

static void process_MACInput() 
{
  char *buffer = new char[12+5+1]{'\0'}; //12 digits + 5 seperators(:) + null-terminator
  char info[250];        //info for user, if input didnt fit to request
  byte _mac[6] = {0, 0, 0, 0, 0, 0};
  uint8_t i = 0, t = 0;

  while (Serial.available()) Serial.read();
  Serial.print(F("\nGeben Sie jetzt die neue MAC-Adresse im Format XX:XX:XX:XX:XX:XX ein oder kehren sie mit \"C\" zurück zum Menü.\n"));

  //1: read input
  while(!Serial.available()) {vTaskDelay(1);}
  if (Serial.peek() == 'C') return;
  Serial.readBytesUntil('\n', buffer, 18);

  //2: validate input as MAC address:
  char *token = strtok(buffer, ":");
  while(token != NULL) 
  {
    //check if if token is a hex number
    if (sscanf(token, "%hhx", &_mac[i]) != 1) 
    {
      Serial.print(F("\nUngültige Eingabe. Bitte geben Sie nur Hexadezimalzahlen und Doppelpunkte ein.\n"));
      delete[] buffer;
      process_MACInput(); 
      return;
    }
    i++;
    token = strtok(NULL, ":");
  }
  if (i != 6) //check if 4 tuples have been entered
  {
    Serial.print(F("\nUngültige Eingabe. Bitte geben Sie 6 Zahlen ein.\n"));
    delete[] buffer;
    process_MACInput(); 
    return; 
  }

  //3: ask user if input is correct

  Serial.printf("\nIhre Eingabe: %02X:%02X:%02X:%02X:%02X:%02X\n", _mac[0], _mac[1], _mac[2], _mac[3], _mac[4], _mac[5]);
  Serial.print(F("\nIst diese Eingabe korrekt? (j/n)"));

  while(!Serial.available()){vTaskDelay(1);}
  memset(&buffer[0], '\0', 3); 
  Serial.readBytesUntil('\n', buffer, 1); 
  Serial.printf("\n%s", buffer);
  if (strcmp(buffer, "j") == 0 || strcmp(buffer, "J") == 0) 
  {
    preferences.begin("lokSim3D_net", false);
    preferences.putBytes("mac", _mac, 6);
    preferences.end();
    Serial.println(F("\nEinstellung gespeichert.\nMenü wird erneut aufgerufen..."));
  }
  else {
    delete[] buffer;
    process_MACInput();
  }
  delete[] buffer;
}

//===============================================================================
//let user change keyboard shortcut / tcp address of an input / output
//not working yet for loksim3d config (keyboard shortcuts & address for tcp)
//===============================================================================

static void opt_1() {
                  
  uint8_t i, t;
  int i2c_adr;
  int pin;
  int new_address;
  char *buffer = new char[3]{'\0'};     //buffer for user-input 
  char current_address[3] = {'\0'};        
  char info[250] = {'\0'};               //info for user, if input didnt fit to request, static array, because there is enough memory, but memory allocation costs cpu cycles


  Serial.print(F("Sie können jederzeit zum Menü zurückgelangen. Geben Sie dazu ein \"C\" ein."));
  Serial.printf("\nGeben Sie die I2C-Addresse des ICs ein, dessen Port-Konfiguration Sie ändern möchten (%i-%i & %i-%i (DEC)): ", MCP_I2C_BASE_ADDRESS, MCP_I2C_END_ADDRESS, PCF_I2C_BASE_ADDRESS, PCF_I2C_END_ADDRESS);   
  strcpy(info, "\nDiese Adresse steht nicht zur verfügung.\nGeben Sie eine passende Adresse ein, oder kehren Sie mit \"C\" zurück zum Menü.\n");
  while (1)  //let user input some valid i2c adress or "C" to return back to the menu. if user entered a valid address, search the index in the lists where this adress is lokated at
  {  
    if (get_integer_with_range_check(&i2c_adr, MCP_I2C_BASE_ADDRESS, PCF_I2C_END_ADDRESS, info)) return;
    
    i = i2c_adr - MCP_I2C_BASE_ADDRESS; //assume, entered address belongs to an mcp ic, i = index in mcp_list 
    if (i < 9) {break;} else { i = i2c_adr - PCF_I2C_BASE_ADDRESS;} //yes? break while loop otherwhise check if it belongs to pcf ic (i = unsigned! cant be < 0)
    if (i < 9)  break; //yes? break, otherwhise go back to get_integer_with_range_check (user entered a number between 40 and 71)

    Serial.printf("%s", info);
  }

  Serial.print(F("\n\nGeben Sie den Pin ein, dessen Kanalnummer Sie ändern möchten."));
    
  if (i2c_adr < MCP_I2C_END_ADDRESS)  //mcp ic        //ask user, which pin he wants to change and then copy current address 
  {
    Serial.print(F("\nDabei entspricht eine Nummer von 0 bis 7 einem Pin an Port A. Höhere Nummern entsprechen einem Pin an Port B.\n Achtung! Es wird nicht überprüft, ob derselbe Kanal schon bei einem anderen I/O verwendet wird!\n")); 
    strcpy(info,  "\nDiesen Pin gibt es nicht\nGeben sie eine Nummer von 0 bis 15 für den Pin ein, den Sie ändern möchten oder kehren Sie mit \"C\" zurück zum Menü.\n");
    if (get_integer_with_range_check(&pin, 0, 15, info)) return; 

    strcpy(current_address, mcp_list[i].address[pin]);
    Serial.printf("\n\nAktuell ist für diesen I/O Kanal %s hinterlegt.\nGeben Sie jetzt eine neue Kanalnummer ein:", current_address);
    memset(&buffer[0], '\0', 3);
    strcpy(info,  "\nDiese Kanalnumer gibt es nicht\nGeben sie eine Nummer von 0 bis 99 ein oder kehren Sie mit \"C\" zurück zum Menü.\n");
    if (get_integer_with_range_check(&new_address, 0, 99, info)) return;

    itoa(new_address, buffer, 10); //convert int to string
    strncpy(mcp_list[i].address[pin], buffer, 2); //write new address to mcp list
    

  } else if (i2c_adr > PCF_I2C_BASE_ADDRESS) //pcf ic
  {
    Serial.print(F("\nDabei entspricht eine Nummer von 0 bis 3 einem der 4 analogen Eingänge. Eine 4 führt entsprechend zur Änderung des Kanals des analogen Ausgangs.\nAchtung! Es wird nicht überprüft, ob derselbe Kanal schon bei einem anderen I/O verwendet wird!\n"));
    memset(&buffer[0], '\0', 3);
    strcpy(info,  "\nDiesen Pin gibt es nicht\nGeben sie eine Nummer von 0 bis 4 für den Pin ein, den Sie ändern möchten oder kehren Sie mit \"C\" zurück zum Menü.\n");
    if (get_integer_with_range_check(&pin, 0, 4, info)) return;
    strcpy(current_address, pcf_list[i].address[pin]);

    Serial.printf("\n\nAktuell ist für diesen I/O Kanal %s hinterlegt.\nGeben Sie jetzt eine neue Kanalnummer ein.", current_address);
    memset(&buffer[0], '\0', 3);
    strcpy(info,  "\nDiese Kanalnumer gibt es nicht\nGeben sie eine Nummer von - bis 99 ein oder kehren Sie mit \"C\" zurück zum Menü.\n");
    if (get_integer_with_range_check(&new_address, -1, 99, info)) return;

    itoa(new_address, buffer, 10); //convert int to string
    strncpy(pcf_list[i].address[pin], buffer, 2);
  }
    
  Serial.print(F("\n\nKanalnummer erfolgreich aktualisiert.\n"));
 
  while (Serial.available()) {Serial.read();}
  Serial.print(F("\nMenü erneut aufrufen? (j/n)"));
  Serial.print(F("\n(Eine andere Eingabe als j wird als Nein interpretiert)\n"));
  while(!Serial.available()){vTaskDelay(1);}

  memset(&buffer[0], '\0', 3); 
  Serial.readBytesUntil('\n', buffer, 1); 
  Serial.printf("\n%s", buffer);
  if (strcmp(buffer, "j") == 0) run_config_task = 1;
  delete[] buffer;

}

//====================
//Network settings
//====================

static void network_settings() 
{
  int option = 0;
  char info[145];
  char *buffer = new char[3]{'\0'}; //two chars and null-terminator

  while (Serial.available() > 0) {Serial.read();}
  run_config_task = 1; 

  Serial.print(F("Was möchten Sie tun?\nAchtung! Änderungen an den Netzwerkeinstellungen können dazu führen, dass das Fahrpult nicht mehr erreichbar ist.\n Änderungen werden direkt gespeichert und sind nach einem Neustart aktiv.\n"));
  Serial.print(F("1 -> Server-IP anpassen\n2 -> MAC-Adresse anpassen\n3 -> Port anpassen\nC -> Zurück zum Menü\n"));

  strcpy(info, "Diese Option steht nicht zur verfügung.\nGeben Sie eine der Verfügbaren Optionen ein, oder beenden Sie mit \"C\" das Konfigurationsmenü.\n");
  if (get_integer_with_range_check(&option, 1, 3, info)) return;   //exit menu, if user entered "C"

  switch (option) 
  {
    case 1:
      Serial.printf("\nAktuell gespeicherte Server-IP: %s\n", host.toString());
      process_IPInput();
      break;
    case 2:
        Serial.printf("\nAktuelle MAC-Adresse: %02X:%02X:%02X:%02X:%02X:%02X\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        process_MACInput();
      break;
    case 3:
      Serial.printf("\nAktueller Port: %u", port);
      Serial.read();
      if (get_integer_with_range_check(&option, 0, INT16_MAX, info)) return;
      preferences.begin("APP", false, "nvs");
      preferences.putUInt("port", option);
      Serial.printf("\nPort erfolgreich geändert auf %i", preferences.getUInt("port", 0)); //read back to check if it was saved correctly
      preferences.end();
      break;
    default:
      Serial.read();
      Serial.print(F("Bitte eine Ziffer von 0 bis 9 für die entsprechende Option eingeben. Erneut versuchen, oder Konfiguration beenden? (j/n)"));
      Serial.print(F("\n(Eine andere Eingabe als J wird als Nein interpretiert)\n"));
      while(Serial.available() < 1) {vTaskDelay(1);}
      if (strcmp(buffer, "j") == 0) run_config_task = 1;
      break;
  }

  delete[] buffer; 

}

//============================
//Serial Configuration Menu
//============================

void serial_config_menu()
{

    int option = 0;
    char info[145];
    char *buffer = new char[3]{'\0'}; //two chars and null-terminator

    while (Serial.available() > 0) {Serial.read();}
     
    Serial.print(F("\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n"));
    Serial.print(F("\nUm Änderungen dauerhaft im Flash-Speicher zu sichern, muss das Konfigurationsmenü über die Option 7 beendet werden. Bei einem Neustart werden ungesicherte Änderungen verworfen.\n"));
    Serial.print(F("Auswahlmöglichkeiten zur Fehlersuche und Konfiguration des Fahrpults: \n\n"));  
     Serial.print(F("1 -> Kanalnummer eines Ein-/ Ausgangs ändern. Es muss die I2C Addresse des dazugehörigen ICs und die Pin-Nummer bekannt sein.\n"));
    if(VERBOSE){
        Serial.print(F("2 -> Debugging Ausgabe Ausschalten.\n"));
    }else{ 
        Serial.print(F("2 -> Debugging Ausgabe Einschalten.\n"));
    }
    Serial.print(F("3 -> Änderungen Speichern und Konfigurationsmenü beenden.\n"));
    Serial.print(F("4 -> Konfigurationsmenü beenden.\n"));
    Serial.print(F("5 -> Neustart. Nicht gesicherte Änderungen gehen verloren!\n"));
    Serial.print(F("6 -> Schnittstelle für die Kommunikation mit einem Simulator wählen.\n\n"));

    Serial.print(F("7 -> Netzwerkeinstellungen ändern\n"));

    strcpy(info, "Diese Option steht nicht zur verfügung.\nGeben Sie eine der Verfügbaren Optionen ein, oder beenden Sie mit \"C\" das Konfigurationsmenü.\n");
    if (get_integer_with_range_check(&option, 1, 7, info)) option = 4;   //exit menu, if user entered "C"
    
    delete[] buffer;    //dead variable, not used 
    
    Serial.print("\n\n");
    Serial.print("-------------------------------------------------");
    Serial.print("-------------------------------------------------\n\n\n");

    switch (option)
    {
        case 1:
            Serial.println("FUNKTION IST WURDE FÜR LOKSIM3D NOCH NICHT IMPLEMENTIERT");
            vTaskDelay(pdMS_TO_TICKS(1000));
            run_config_task = 1;
            //opt_1();
            break;
        case 2:
            toggle_verbose();
            break;
        case 3:
            Serial.println("FUNKTION IST UNGETESTET. NUTZUNG AUF EIGENE GEFAHR!");
            vTaskDelay(pdMS_TO_TICKS(1000));
            commit_config_to_fs(2); //number indicates, which sim_interface is active 
            break;
        case 4: //exit
            break;
        case 5:
            ESP.restart();
            break;
        case 6: 
            choose_sim();
            break;
        case 7:
            network_settings();
            break;
        default:
            Serial.read();
            Serial.print(F("Bitte eine Ziffer von 0 bis 9 für die entsprechende Option eingeben. Erneut versuchen, oder Konfiguration beenden? (j/n)"));
            Serial.print(F("\n(Eine andere Eingabe als J wird als Nein interpretiert)\n"));
            while(Serial.available() < 1) {vTaskDelay(1);}
            if (strcmp(buffer, "j") == 0) run_config_task = 1;
            break;
    }

    if (!run_config_task) 
    {
      Serial.print(F("\nKonfigurationsmenü beendet.\nUm in das Konfigurationsmenü zu gelangen bitte \"M\" eingeben."));
      Serial.print(F("\n\n-------------------------------------------------"));
      Serial.print(F("-------------------------------------------------\n\n\n\n\n\n\n\n"));
    }
}

}; //namespace ConfigMenu

#endif 
//================================================================================================================
//let user enter a number with specific bounds and check if input is valid, user can exit to menu by entering "C"
//================================================================================================================
static bool get_integer_with_range_check (int *var, uint lower_bound, uint upper_bound, char *info) //returns true (exit to menu) if user entered "C"
{  
  int bytesRead = 0;
  const int max_bytes = 3; 
  char* buffer = new char[max_bytes + 1]; //input buffer for user input (3 chars + null-terminator)

  while (Serial.available()) Serial.read(); //clear Serial hw buffer
  while(!Serial.available()) {vTaskDelay(1);} //wait for user-input
  if (Serial.peek() == 'C' || Serial.peek() == 'c' ) return true;

  //max 3 chars allowed, but user could enter less (e.g. if user is prompted to enter a number between 0 and 10)
  while (Serial.available()) 
  {
    bytesRead += Serial.readBytesUntil('\0', buffer + bytesRead, max_bytes - bytesRead);  
    if (bytesRead > 0 || buffer[0] == '\0') break;
    vTaskDelay(1); 
  }

  buffer[bytesRead] = '\0'; 
  Serial.printf("\n%s", buffer);

  int error = sscanf(buffer, "%u", var);

  //check if input is a number and within bounds
  //if not, ask user to enter a number again until input is valid or user exits to menu
  while (error != 1 || *var < lower_bound || *var > upper_bound) 
  {
    Serial.printf("\nIhre Eingabe: %s", buffer);
    memset(buffer, '\0', 16 + 1); 
    Serial.printf("\n%s\n", info);

    while (Serial.available()) Serial.read();

    while (!Serial.available()) vTaskDelay(1); //wait for user-input
        
    if (Serial.peek() == 'C') return true;
        
    bytesRead = 0;
    while (Serial.available()) 
    {
      bytesRead += Serial.readBytesUntil('\0', buffer + bytesRead, max_bytes - bytesRead);
      if (bytesRead > 0 || buffer[0] == '\0') break;
      vTaskDelay(1); 
    }

    buffer[bytesRead] = '\0'; 
    Serial.printf("\n%s", buffer);
    error = sscanf(buffer, "%u", var);

    if (error != 1) Serial.println("Bitte eine Nummer eingeben."); 
      
  }

  while (Serial.available()) Serial.read(); //clear Serial hw buffer

  return false;
}

//=======================================================
//file utils for load_config() and commit_config_to_fs()
//=======================================================
static  size_t getFilesize(fs::FS &fs, const char* path) 
{
  size_t fsize;
  File file = fs.open(path);
  if (!file || file.isDirectory()) {
    Serial.print(F("- Datei konnte nicht geöffnet werden.\n"));
    return 0;
  }
  fsize = file.size(); //get filesize in bytes
  file.close();
  return fsize;
}

static void readFile(fs::FS &fs, const char *path, char *buffer, size_t len) 
{
  Serial.printf("Datei wird geöffnet: %s\r\n", path);

  File file = fs.open(path);
  if (!file || file.isDirectory()) {
    Serial.print(F("- Datei konnte nicht geöffnet werden.\n"));
    return;
  }
  while (file.available()) {
    file.readBytes(buffer, len);
  }
  file.close();
}

static void appendFile(fs::FS &fs, const char *path, const char *message) 
{
  //Serial.printf("Neue Daten in %s anhängen\r\n", path);

  File file = fs.open(path, FILE_APPEND);
  if (!file) {
    Serial.print(F("- Datei konnte nicht geöffnet werden.\n"));
    return;
  }
  if (file.print(message)) {
    //Serial.println("- neue Daten erfolgreich eingefügt.");
  } else {
    Serial.println("- neue Daten konnten nicht angefügt werden.");
  }
  file.close();
}

static void clearFile(fs::FS &fs, const char *path) 
{
    File file = fs.open(path, FILE_WRITE);
    if (!file) {
        Serial.println("Datei konnte nicht geöffnet werden.");
        return;
    }
    file.close();  // Truncate the file to zero length
}

//=======================================================================================================================================
//print io configuration of a mcp-IC in binary format (only used in config_task, because its not using a queue but printing directly with Serial)
//=======================================================================================================================================

void printBinary(uint16_t value) 
{
    
    for (int t = 0; t < 16; t++) { //print portMode in binary format
      Serial.printf("%i", (CHECK_BIT(value, t)) ? 1 : 0 ); //at Serial monitor, bit 0 represents pin 0 and bit 15 pin 15
    }
    Serial.println();
}

//======================================================================
//option 2 for config menu in config_file_utils 
//======================================================================
static void toggle_verbose() 
{
  if(VERBOSE) 
    {
        VERBOSE = 0;
        Serial.print(F("Debugging-Ausgabe ausgeschaltet."));
    } 
    else {
        VERBOSE = 1;
        Serial.print(F("Debugging-Ausgabe eingeschaltet."));
    }
    run_config_task = 1;
}

//======================================================================
//option 6 for config menu in config_file_utils
//======================================================================

static void choose_sim() 
{
  char info[250] = {'\0'};
  int option = 0;
   
  Serial.print(F("Kehren Sie mit \"C\" zurück zum Menü.\n"));   
  Serial.print(F("Welche Schnittstelle soll nach dem nächsten Neustart geladen werden?\n\n1 -> THM-Simulator\n2 -> LokSim3D\n"));

  strcpy(info,  "\nDiese Option steht nicht zur verfügung.\nGeben Sie eine \"1\" oder eine \"2\" ein, oder kehren Sie mit \"C\" zurück zum Menü.\n");
  if (get_integer_with_range_check(&option, 1, 2, info)) return;
  
  preferences.begin("APP", false, "nvs");

  if (option == 1) 
  {
    preferences.putUInt("app_settings", option);
    
    Serial.print(F("\nSchnittstelle zu THM-Simulator wird dem nächsten Neustart initialisiert."));
  } 
  else if (option == 2) 
  {
    preferences.putUInt("app_settings", option);
    Serial.print(F("\nSchnittstelle zu LokSim3D wird dem nächsten Neustart initialisiert."));
  } 
  preferences.end();

  if (option) run_config_task = 1; //return to menu, where user can reset the device with option 9
}

//======================================================================
//store changes in config files to flash
//======================================================================
static void commit_config_to_fs(int sim) //overwrite specific data depending on simulator //1: thmSim, 2: lokSim3D
{ 
  if (sim != 1 && sim != 2) {
    Serial.printf("%i ist keine Option für \"commit_config_to_fs(int sim)\" ", sim);
    return;
  }

  char buffer[2] = {'\0', '\0'};
  Serial.print(F("\"S\" - Um die Änderungen im Flash zu sichern und die alte Konfiguration zu überschreiben, geben Sie ein \"S\" ein."));
  Serial.print(F("\nKehren Sie mit \"C\" zurück zum Menü."));   
  
  while(Serial.available() < 1) { vTaskDelay(10); }
  Serial.readBytesUntil('\n', buffer, 1);
  Serial.println(buffer);

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
    
    Serial.print(F("\nAktualisiere mcp.txt..."));
    update_mcp_config(current_mcp_config, sim, mcp_buf_size);
  
    Serial.print(F("\nAktualisiere pcf.txt..."));
    update_pcf_config(current_pcf_config, sim, pcf_buf_size);

    //=========================================================

    Serial.print(F("\nAktualisierung erfolgreich abgeschlossen."));
    delete[] current_mcp_config;
    delete[] current_pcf_config;
  }

}
//==========================================================================================================================
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
    
  Serial.print(F("\n Neue Konfiguration wird erstellt. Bitte warten..."));
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
        if (VERBOSE) Serial.println("Kein \"key\" oder \"adresse\" Eintrag gefunden. Alter Wert wird übernommen.");
        len = snprintf(NULL, 0, "%u;%u;%s;%u;%s;%s;%s\n", i2c[t], pin[t], kanal[t], io[t], key[t], address[t], mcp_list[ i2c[t] ].info[ pin[t] ] ) + 1; //+1 for null-terminator
        buffer = new char[len];
        snprintf(buffer, len, "%u;%u;%s;%u;%s;%s;%s\n", i2c[t], pin[t], kanal[t], io[t], key[t], address[t], mcp_list[ i2c[t] ].info[ pin[t] ] );
      }
      //store row
      strncpy(updated_config, buffer, len);
      if (VERBOSE) Serial.printf("%s\n", buffer); 
    }
  }
  delete[] buffer;

  //overwrite config file
  Serial.println("Alte Konfiguration wird überschrieben. Bitte warten...");
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
static void update_pcf_config(char *current_config, int sim, size_t config_size)
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
    
  Serial.print(F("\n Neue Konfiguration wird erstellt. Bitte warten..."));
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
        if (VERBOSE) Serial.println("Kein \"key\" oder \"adresse\" Eintrag gefunden. Alter Wert wird übernommen.");
        len = snprintf(NULL, 0, "%u;%u;%s;%s;%s;%s\n", i2c[t], pin[t], kanal[t], key[t], address[t], mcp_list[ i2c[t] ].info[ pin[t] ] ) + 1; //+1 for null-terminator
        buffer = new char[len];
        snprintf(buffer, len, "%u;%u;%s;%s;%s;%s\n", i2c[t], pin[t], kanal[t], key[t], address[t], mcp_list[ i2c[t] ].info[ pin[t] ] );
      }
      //store row
      strncpy(updated_config, buffer, len);
      if (VERBOSE) Serial.printf("%s\n", buffer);   
    }
  }
  delete[] buffer;

  //overwrite config file
  Serial.println("Alte Konfiguration wird überschrieben. Bitte warten...");
  clearFile(LittleFS, "/pcf.txt");
  
  //dont update file at once but row for row, because it could overload the ram which would lead to errors
  appendFile(LittleFS, "/pcf.txt", pcf_csvHeader);
  char *row = strtok(updated_config, "\n"); 
  while (row != NULL) {
    appendFile(LittleFS, "/pcf.txt", row);
    row = strtok(NULL, "\n"); //extract next row
  }
}

