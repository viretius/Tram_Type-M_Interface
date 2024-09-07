#ifndef TRAINSIM_CONFIG_FILE_UTILS
#define TRAINSIM_CONFIG_FILE_UTILS

#include <globals.h>                  
#include <utils.h>
#include <thmSim/thmSim_interface.h> //import namespace thmSim_interface

namespace thmSim_config { 
    bool load_config();
    void serial_config_menu();
};

#endif