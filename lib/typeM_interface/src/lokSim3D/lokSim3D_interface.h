#ifndef LOKSIM3D_INTERFACE_H 
#define LOKSIM3D_INTERFACE_H
#if ( ARDUINO_NANO_ESP32)


#include <globals.h>                  
#include <config_file_utils.h>
#include <USBHIDKeyboard.h>

#define DEBUG_ETHERNET_WEBSERVER_PORT  Serial
#define _ETHERNET_WEBSERVER_LOGLEVEL_  3

//not using default SPI-pins for int and cs
//these are gpio numbers, not pin numbers printed on the board
#define INT_GPIO            17 
#define CS_GPIO             18
#define MISO_GPIO           47
#define MOSI_GPIO           38
#define SCK_GPIO            48
//include the library for the W5500 module after pin definitions,
//because the library uses the default SPI-pins otherwise
#include <WebServer_ESP32_SC_W5500.h>

extern byte mac[];      
extern IPAddress host;
extern uint16_t port;     //default is 1435 for LOKSIM3D  
//no client ip - dhcp is used    

namespace lokSim3D_interface {   

    //============================================================
    //loksim3d specific variables and definitions
    //============================================================

    constexpr int GESCHWINDIGKEIT = 1;
    constexpr int AFB_SOLL_GESCHWINDIGKEIT = 18;
    constexpr int LM_Hauptschalter = 37;
    constexpr int LM_MG_BREMSE = 41;
    constexpr int LM_H_BREMSE = 42;
    constexpr int LM_R_BREMSE = 43;
    constexpr int LM_TUEREN = 47;

    constexpr int NUM_STATS = 7; //number of requested data-sets (see above)
    
    //transmitted once after connection is established
    extern byte handshake[]; 
    extern size_t handshake_len;
    
    //transmitted once after handshake[]-transmission
    extern byte request[];    
    extern size_t request_len; 

    //transmitted after request[]-transmission - terminates the request
    extern byte request_end[];
    extern size_t request_end_len;
 

    static USBHIDKeyboard Keyboard;
    static WiFiClient client;

    static QueueHandle_t tcp_rx_queue;
    static QueueHandle_t keyreport_tx_queue;

    //struct to store received data-pakets from tcp server
    typedef struct {
        char *message;
        int count;     //number of outputs, of which some data inside "message" belongs to
    } tcp_payload;

    //================================================
    //================================================

    void init_interface();  

};

#endif
#endif