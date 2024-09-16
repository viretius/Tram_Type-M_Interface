#ifndef CONFIG_FILE_UTILS
#define CONFIG_FILE_UTILS

#include <globals.h>                  

#include <thmSim/thmSim_interface.h> 
#if ( ARDUINO_NANO_ESP32)
#include <lokSim3D/lokSim3D_interface.h>
#endif 

void printBinary(uint16_t value);

namespace thmSim_config { 
    bool load_config();
    void serial_config_menu();
};

#if ( ARDUINO_NANO_ESP32)
namespace lokSim3D_config{  //only available for ESP32-S3
    bool load_config();
    void serial_config_menu();
};
#endif 

#endif