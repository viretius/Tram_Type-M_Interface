/****************************************************************************************************************************
  WebServer_ESP32_SC_W5500_Impl.h

  For Ethernet shields using ESP32_SC_W5500 (ESP32_S2/S3/C3 + LwIP W5500)

  WebServer_ESP32_SC_W5500 is a library for the ESP32_S2/S3/C3 with LwIP Ethernet W5500

  Based on and modified from ESP8266 https://github.com/esp8266/Arduino/releases
  Built by Khoi Hoang https://github.com/khoih-prog/WebServer_ESP32_SC_W5500
  Licensed under GPLv3 license

  Version: 1.2.1

  Version Modified By   Date      Comments
  ------- -----------  ---------- -----------
  1.0.0   K Hoang      13/12/2022 Initial coding for ESP32_S3_W5500 (ESP32_S3 + W5500)
  1.0.1   K Hoang      14/12/2022 Using SPI_DMA_CH_AUTO instead of manually selected
  1.1.0   K Hoang      19/12/2022 Add support to ESP32_S2_W5500 (ESP32_S2 + W5500)
  1.2.0   K Hoang      20/12/2022 Add support to ESP32_C3_W5500 (ESP32_C3 + W5500)
  1.2.1   K Hoang      22/12/2022 Remove unused variable to avoid compiler warning and error
 *****************************************************************************************************************************/

#pragma once

#ifndef WEBSERVER_ESP32_SC_W5500_IMPL_H
#define WEBSERVER_ESP32_SC_W5500_IMPL_H

#include <Arduino.h>
#include "WebServer_ESP32_SC_W5500_Debug.h"
#include "esp_log.h"
#include "esp_check.h"
#include "w5500/esp32_sc_w5500.h"
#include "esp_event.h"

//////////////////////////////////////////////////////////////

extern bool ESP32_W5500_eth_connected;

void ESP32_W5500_onEvent();

//////////////////////////////////////////////////////////////

void ESP32_W5500_waitForConnect();

//////////////////////////////////////////////////////////////

bool ESP32_W5500_isConnected();

//////////////////////////////////////////////////////////////

void ESP32_W5500_event(WiFiEvent_t event);

//////////////////////////////////////////////////////////////

#endif    // WEBSERVER_ESP32_SC_W5500_IMPL_H
