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

void queue_printf(QueueHandle_t queue, const int size, const char *format, ...) {
    
    char buffer[size];
    va_list args;
    va_start(args, format); //format -> last argument before arg-list
    vsnprintf(buffer, size, format, args);
    va_end(args);
    
    if (xQueueSend(queue, &buffer, pdMS_TO_TICKS(100)) != pdTRUE) {
      if (VERBOSE) USBSerial.print("Queue is full. Message dropped.");
    }
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

//==========================================================================================================================
//let user enter a number with specific bounds and max 16 digits
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