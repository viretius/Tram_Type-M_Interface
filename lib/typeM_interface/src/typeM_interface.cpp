#include <typeM_interface.h>


void init_typeM_interface() {
    
    //USB.onEvent(USB_Event_Callback); //for debugging purposes - USBSerial rx functionality wont work if this line is uncommented
    //USBSerial.onEvent(USB_Event_Callback); 

    USBSerial.begin();
    USB.begin();
    while(!USBSerial);
    find_and_print_partitions();
    
    //====================================================================================
    //actual application initialization
    //====================================================================================

    //load variable from eeprom to determine which interface to use
    preferences.begin("APP", false, "nvs");
    uint8_t interface_choice = preferences.getUInt("app_setting", 0);
    preferences.end();
    if(interface_choice == 2) {
        USBSerial.print(F("LokSim3D Setup...\n"));
        lokSim3D_interface::init();
    } else  {
        USBSerial.print(F("SimMetro Setup...\n"));
        simMetro_interface::init();
    }
}

