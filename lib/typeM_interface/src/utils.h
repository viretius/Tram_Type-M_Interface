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
//functions called by serial_config_menu in config_file_utils 
//==============================================================================================
//functions to change simulator specific settings / configurations are implemented in config_file_utils directly 
//yeayea i was to lazy to give them proper names
//==============================================================================================
void opt_1(); //call it debug output
void opt_2(); //print current config with changes made by user to serial monitor
void opt_3(); //change port config (input/output) of an mcp ic
void opt_5(); //change number of active ICs

void toggle_verbose();//option 6 to toggle verbose output

void commit_config_to_fs();//option 7 for config menu in config_file_utils

void opt_10();//choose which interface should be loaded after reboot

//==============================================================================================
//let user know that setup is finished by blinking all leds
//==============================================================================================
void indicate_finished_setup();

//==============================================================================================
//print partitions to serial monitor (called at setup)
//==============================================================================================
void find_and_print_partitions();

#endif