#ifndef TRAINSIM_TYPEM_INTERFACE_H 
#define TRAINSIM_TYPEM_INTERFACE_H


#include <utils.h>
#include <simMetro_interface/config_file_utils.h>

//================================================================================================================
//================================================================================================================


namespace simMetro_interface {
    
    void init();

    static QueueHandle_t serial_rx_cmd_queue;
    static QueueHandle_t serial_tx_cmd_queue;
};

#endif