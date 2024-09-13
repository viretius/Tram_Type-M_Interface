#ifndef UTILS_H
#define UTILS_H

#include "Arduino.h"
#include <globals.h>

#include <USB.h>
#include <USBHIDKeyboard.h>

//==============================================================================================
//funktion like sprintf(). prints a formatted string to a queue, instead of a c-String variable
//used for verbose output
//==============================================================================================

template <size_t count> //template to allow dynamic buffer size
void queue_printf(QueueHandle_t queue, const char *format, ...) {
    
    static char buffer[count] = {'\0'}; 

    va_list args;
    va_start(args, format);
    vsnprintf(buffer, count, format, args);
    va_end(args);
    
    // Send buffer contents to the queue
    if (xQueueSend(queue, &buffer, pdMS_TO_TICKS(10)) != pdTRUE) {
        USBSerial.println("Queue full\n");
        USBSerial.print(buffer);
    }
}
//==============================================================================================
//helperfunctions for config_file_utils
//==============================================================================================

size_t getFilesize(fs::FS &fs, const char* path);

void readFile(fs::FS &fs, const char *path, char *buffer, size_t len);

void appendFile(fs::FS &fs, const char *path, const char *message);

void clearFile(fs::FS &fs, const char *path);

void printBinary(uint16_t value);

bool get_integer_with_range_check(int *var, uint lower_bound, uint upper_bound, char *info);

//==============================================================================================
//functions called by serial_config_menu in config_file_utils 
//==============================================================================================

void toggle_verbose();

void commit_config_to_fs(int sim);

void choose_sim();  //choose which interface should be loaded after next restart

//==============================================================================================
//print partitions to serial monitor 
//==============================================================================================

void find_and_print_partitions();

#endif