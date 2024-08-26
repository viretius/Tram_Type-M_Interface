#pragma once

#include "Arduino.h"
#include <config.h>

#include <USB.h>
#include <USBHIDKeyboard.h>

void USB_Event_Callback(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

/**
*
*funktion like sprintf(). prints a formatted string to a queue, instead of a c-String variable
*used for verbose output and 
*/

void queue_printf(QueueHandle_t queue, const int size, const char *format, ...);

/*
*functions for file/data management
*openFile() functions return a File object. 
*This object supports all the functions of Stream, 
*so you can use readBytes, findUntil, parseInt, println, and all other Stream methods
*/ 

size_t getFilesize(fs::FS &fs, const char* path);

void readFile(fs::FS &fs, const char *path, char *buffer, size_t len);

void appendFile(fs::FS &fs, const char *path, const char *message);

void clearFile(fs::FS &fs, const char *path);


