#include <typeM_interface.h>


void init_typeM_interface() {
    
    //USB.onEvent(USB_Event_Callback); //for debugging purposes - USBSerial rx functionality wont work if this line is uncommented
    //USBSerial.onEvent(USB_Event_Callback); 
    //HWSerial.setHwFlowCtrlMode(UART_HW_FLOWCTRL_DISABLE, 64);
    //HWSerial.begin(115200);
    USBSerial.begin();  
    USB.begin();
    while(!USBSerial);
    find_and_print_partitions();
    
    //====================================================================================
    //actual application initialization
    //====================================================================================

    //load variable from NVS to determine which interface to use
    preferences.begin("APP", true, "nvs");
    uint8_t interface_choice = preferences.getUInt("app_settings", 0);
    preferences.end();
    if(interface_choice == 2) {
        USBSerial.print(F("Schnittstelle wird für LokSim3D initialisiert...\n"));
        lokSim3D_interface::init();
    } else if (interface_choice == 1) {
        USBSerial.print(F("Schnittstelle wird für THM-Simulator initialisiert...\n"));
        thmSim_interface::init();
    }
    else {
        USBSerial.println("Variable für die Wahl des Simulators entspricht keinem der zulässigen Werte.");
        USBSerial.println("Wählen Sie im Konfigurationsmenü Option 6, um die Variable anzupassen und führen dann einen Neustart durch");
        thmSim_config::serial_config_menu();
    }
}   

