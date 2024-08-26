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
    
    xQueueSend(queue, &buffer, pdMS_TO_TICKS(2));
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



