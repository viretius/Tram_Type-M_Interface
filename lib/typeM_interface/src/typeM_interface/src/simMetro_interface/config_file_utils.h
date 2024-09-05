#ifndef TRAINSIM_CONFIG_FILE_UTILS
#define TRAINSIM_CONFIG_FILE_UTILS

#include <config.h>
#include <utils.h>
#include <simMetro_interface/simMetro_interface.h> //mcp_list and pcd_list


namespace simMetro_config{
    
    bool load_config();
    void serial_config_menu();
};



#endif