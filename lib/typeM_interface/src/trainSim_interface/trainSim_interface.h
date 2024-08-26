#ifndef TRAINSIM_TYPEM_INTERFACE_H 
#define TRAINSIM_TYPEM_INTERFACE_H


#include <config.h>
#include <utils.h>

#include <trainSim_interface/config_file_utils.h>

//================================================================================================================
//================================================================================================================


namespace trainSim_interface {

    //============================================================ 
    //various global variables and objects
    //============================================================
    
    //============================================================
    //func declarations
    //============================================================  
    
    void indicate_finished_setup();
    void toggle_buzzer(uint8_t pin, float frequency);
    uint8_t calc_throttle_position(uint8_t value);

    //============================================================
    //rtos stuff
    //============================================================

    static QueueHandle_t serial_rx_cmd_queue;       
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