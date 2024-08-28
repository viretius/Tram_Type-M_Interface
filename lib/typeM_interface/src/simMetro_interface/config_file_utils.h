#ifndef TRAINSIM_CONFIG_FILE_UTILS
#define TRAINSIM_CONFIG_FILE_UTILS

#include <simMetro_interface/simMetro_interface.h> //mcp_list and pcd_list

#include <FS.h>
#include <LittleFS.h>
#include <FFat.h>
#include <CSV_Parser.h>

namespace simMetro_config{
    bool load_config();
    void serial_config_menu();
};



#endif