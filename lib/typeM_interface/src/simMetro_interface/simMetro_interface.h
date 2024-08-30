#ifndef TRAINSIM_TYPEM_INTERFACE_H 
#define TRAINSIM_TYPEM_INTERFACE_H


#include <utils.h>
#include <simMetro_interface/config_file_utils.h>

//================================================================================================================
//================================================================================================================


namespace simMetro_interface {

    //============================================================
    //func declarations
    //============================================================  
    
    void init();

    void toggle_buzzer(uint8_t pin, float frequency);
    //============================================================
    //rtos stuff
    //============================================================

    static QueueHandle_t serial_rx_queue;       
    static QueueHandle_t serial_tx_cmd_queue;
 
    void digital_input_task (void * pvParameters);
    void analog_input_task (void * pvParameters);
    void output_task (void * pvParameters);
    void serial_rx_task (void * pvParameters);
    void serial_tx_task (void * pvParameters);
    void config_task (void * pvParameters);

};

#endif
