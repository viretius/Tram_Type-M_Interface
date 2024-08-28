#ifndef TRAINSIM_TYPEM_INTERFACE_H 
#define TRAINSIM_TYPEM_INTERFACE_H


#include <utils.h>
#include <simMetro_interface/config_file_utils.h>

//================================================================================================================
//================================================================================================================


namespace simMetro_interface {

    //============================================================ 
    //various global variables and objects
    //============================================================
    
    //============================================================
    //func declarations
    //============================================================  
    
    void toggle_buzzer(uint8_t pin, float frequency);
    uint8_t calc_throttle_position(uint8_t value);

    //============================================================
    //rtos stuff
    //============================================================

    static QueueHandle_t serial_rx_queue;       
    static QueueHandle_t serial_tx_cmd_queue;

    //============================================================
    //tasks
    //============================================================
        
    void digital_input_task (void * pvParameters);
    void analog_input_task (void * pvParameters);
    void output_task (void * pvParameters);
    void serial_rx_task (void * pvParameters);
    void serial_tx_task (void * pvParameters);
    void config_task (void * pvParameters);

    void init();
};

#endif