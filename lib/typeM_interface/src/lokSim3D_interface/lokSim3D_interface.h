#ifndef LOKSIM3D_TYPEM_INTERFACE_H 
#define LOKSIM3D_TYPEM_INTERFACE_H

//include config file utils in parent folder

#include <config.h>
#include <utils.h>
#include <lokSim3D_interface/config_file_utils.h>



namespace lokSim3D_interface {   
    //============================================================ 
    //various global variables and objects
    //============================================================

    //============================================================
    //USB HID 
    //============================================================

    static USBHIDKeyboard Keyboard;

    //============================================================
    //Network stuff
    //============================================================

    //declare the Ethernet client object
    static WiFiClient client;

    //============================================================
    //func declarations
    //============================================================  

    void indicated_finished_setup();
    void toggle_buzzer(uint8_t pin, float frequency);
    uint8_t calc_throttle_position(uint8_t value);

    //============================================================
    //rtos stuff
    //============================================================

    static QueueHandle_t tcp_rx_cmd_queue;       
    static QueueHandle_t keyboard_tx_queue;

    //============================================================
    //tasks
    //============================================================

    void digital_input_task (void * pvParameters);
    void analog_input_task (void * pvParameters);
    void output_task (void * pvParameters);
    void tcp_rx_task (void * pvParameters);
    void keyboard_tx_task (void * pvParameters);
    void config_task (void * pvParameters);

    void init();

};

#endif