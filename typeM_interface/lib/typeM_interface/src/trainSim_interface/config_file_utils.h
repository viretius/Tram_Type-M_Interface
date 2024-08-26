#ifndef TRAINSIM_CONFIG_FILE_UTILS
#define TRAINSIM_CONFIG_FILE_UTILS

#include <trainSim_interface/trainSim_interface.h> //mcp_list and pcd_list

#include <FS.h>
#include <LittleFS.h>
#include <CSV_Parser.h>

namespace trainSim_config{
    bool load_config();
    void serialConfigMenu();
};



#endif