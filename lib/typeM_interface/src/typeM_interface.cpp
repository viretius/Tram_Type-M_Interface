#include <typeM_interface.h>


void init_typeM_interface() 
{
    Serial.begin(9600); 
    while(!Serial){;} 
 
    #if ( ARDUINO_NANO_ESP32 ) //loksim3d only available for esp32-s3
    USB.begin();
    //wait for USB-Serial connection to be established (DTR signal is expected)

    //load variable from NVS to determine which interface to use
    preferences.begin("APP", true, "nvs");
    uint8_t interface_choice = preferences.getUInt("app_settings", 0);
    
    preferences.end();
    if(interface_choice == 2) {
        Serial.print(F("Schnittstelle wird für LokSim3D initialisiert...\n"));
        lokSim3D_interface::init_interface();
    } else { //default 
        Serial.print(F("Schnittstelle wird für THM-Simulator initialisiert...\n"));
        thmSim_interface::init_interface();
    }
    #else
    thmSim_interface::init_interface();
    #endif

}   
