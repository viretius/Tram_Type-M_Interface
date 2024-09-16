#ifndef TRAINSIM_INTERFACE_H 
#define TRAINSIM_INTERFACE_H

#include <globals.h>                  
#include <config_file_utils.h>

//================================================================================================================
//================================================================================================================

namespace thmSim_interface {
    
    void init_interface();

    static QueueHandle_t serial_rx_cmd_queue;
    static QueueHandle_t serial_tx_cmd_queue;
};

#endif