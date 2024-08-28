#ifndef LOKSIM3D_CONFIG_FILE_UTILS
#define LOKSIM3D_CONFIG_FILE_UTILS

#include <config.h>
#include <utils.h>
#include <lokSim3D_interface/lokSim3D_interface.h>

#include <FS.h>
#include <LittleFS.h>
#include <FFat.h>
#include <CSV_Parser.h>

namespace lokSim3D_config{
    bool load_config();
    void serial_config_menu();
};

#endif