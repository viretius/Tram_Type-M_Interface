#ifndef LOKSIM3D_TYPEM_INTERFACE_H 
#define LOKSIM3D_TYPEM_INTERFACE_H

//include config file utils in parent folder

#include <utils.h>
#include <config.h>
#include <lokSim3D_interface/config_file_utils.h>



namespace lokSim3D_interface {   
    static USBHIDKeyboard Keyboard;

    //declare the Ethernet client object
    static WiFiClient client;

    //============================================================
    //func declarations
    //============================================================  
    
    void init();


    void toggle_buzzer(uint8_t pin, float frequency);
    void handshake_and_request();

    //============================================================
    //rtos stuff
    //============================================================

    static QueueHandle_t tcp_rx_cmd_queue;       
    static QueueHandle_t keyboard_tx_queue; 

    void digital_input_task (void * pvParameters);
    void analog_input_task (void * pvParameters);
    void output_task (void * pvParameters);
    void rx_task (void * pvParameters);
    void tx_task (void * pvParameters);
    void config_task (void * pvParameters);

};

#endif
