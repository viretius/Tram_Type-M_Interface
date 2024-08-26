#include <Arduino.h>
#include <typeM_interface.h>
#include <config.h>

void setup() {
  init_typeM_interface();
}

void loop() {
  //vTaskDelete(NULL); //dont let the rtos task-scheduler reschedule the task, that runs the loop function 
}

