#ifndef UTILS_H
#define UTILS_H

#include "Arduino.h"
#include <config.h>

#include <USB.h>
#include <USBHIDKeyboard.h>


void USB_Event_Callback(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data); //debugging

//==============================================================================================
//funktion like sprintf(). prints a formatted string to a queue, instead of a c-String variable
//used for verbose output and 
//==============================================================================================

void queue_printf(QueueHandle_t queue, const int size, const char *format, ...);

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
//let user know that setup is finished
//==============================================================================================
void indicate_finished_setup();

//==============================================================================================
//print partitions to serial monitor 
//==============================================================================================
void find_and_print_partitions();

#endif