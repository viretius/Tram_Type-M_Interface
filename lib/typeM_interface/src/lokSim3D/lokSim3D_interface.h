#ifndef LOKSIM3D_TYPEM_INTERFACE_H 
#define LOKSIM3D_TYPEM_INTERFACE_H

//include config file utils in parent folder

#include <utils.h>
#include <globals.h>                  
#include <lokSim3D/config_file_utils.h>



namespace lokSim3D_interface {   
   
    void init();  

    static USBHIDKeyboard Keyboard;
    static WiFiClient client;

    static QueueHandle_t tcp_rx_queue;
    static QueueHandle_t keyboard_tx_queue;

};

#endif