#pragma once

#include <Arduino.h>
#include <globals.h> 
 
#include <thmSim/thmSim_interface.h>

#if ( ARDUINO_NANO_ESP32)
#include <lokSim3D/lokSim3D_interface.h>
#endif 

void init_typeM_interface();

