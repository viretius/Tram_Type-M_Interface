/*
https://microchip.my.site.com/s/article/GPA7---GPB7-Cannot-Be-Used-as-Inputs-In-MCP23017

In the datasheet Revision D (June 2022), GPA7 & GPB7 are mentioned
as outputs only for MCP23017. But, the register bits do allow for 
the direction of these I/O pins to be changed to inputs. However, 
the SDA signal can be corrupted during a reading of these bits if 
the pin voltage changes during the transmission of the bit. It has 
also been reported that this SDA corruption can cause some bus hosts 
to malfunction. 

!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!In conclusion, GPA7 & GPB7 should only be used as outputs to avoid future issues.!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

MCP23017 and PCF8591 i2C Addresses based on wiring 

MCP-Address  A2   A1    A0   PCF-Address
    0x20    GND  GND   GND   0x48
    0x21    GND  GND   3V3   0x49
    0x22    GND  3V3   GND   0x4A
    0x23    GND  3V3   3V3   0x4B
    0x24    3V3  GND   GND   0x4C
    0x25    3V3  GND   3V3   0x4D
    0x26    3V3  3V3   GND   0x4E
    0x27    3V3  3V3   3V3   0x4F

*/

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
        USBSerial.print(F("\nLokSim3D Setup..."));
        lokSim3D_interface::init();
    } else  {
        USBSerial.print(F("\nSimMetro Setup..."));
        simMetro_interface::init();
    }


    

}   

