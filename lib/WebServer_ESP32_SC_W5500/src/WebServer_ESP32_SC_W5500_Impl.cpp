#include "WebServer_ESP32_SC_W5500_Impl.h" 


void ESP32_W5500_onEvent()
{
  WiFi.onEvent(ESP32_W5500_event);
}

void ESP32_W5500_waitForConnect()
{
  while (!ESP32_W5500_eth_connected)
  {
    vTaskDelay(pdMS_TO_TICKS(3000));
    Serial.print(".");
  }
}

bool ESP32_W5500_isConnected()
{
  return ESP32_W5500_eth_connected;
}

void ESP32_W5500_event(WiFiEvent_t event)
{
  switch (event)
  {
      //#if USING_CORE_ESP32_CORE_V200_PLUS
#if ( ( defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 2) ) && ( ARDUINO_ESP32_GIT_VER != 0x46d5afb1 ) )
    // For breaking core v2.0.0
    // Why so strange to define a breaking enum arduino_event_id_t in WiFiGeneric.h
    // compared to the old system_event_id_t, now in tools/sdk/esp32/include/esp_event/include/esp_event_legacy.h
    // You can preserve the old enum order and just adding new items to do no harm
    case ARDUINO_EVENT_ETH_START:
      ET_LOG(F("\nETH Started"));
      //set eth hostname here
      ETH.setHostname("ESP32_SC_W5500");
      break;

    case ARDUINO_EVENT_ETH_CONNECTED:
      ET_LOG(F("\nETH Connected"));
      ET_LOG(F("ETH waiting for IP"));
      break;

    case ARDUINO_EVENT_ETH_GOT_IP:
      if (!ESP32_W5500_eth_connected)
      {
        ET_LOG3(F("\nETH MAC: "), ETH.macAddress(), F(", IPv4: "), ETH.localIP());

        if (ETH.fullDuplex())
        {
          ET_LOG0(F("FULL_DUPLEX, "));
        }
        else
        {
          ET_LOG0(F("HALF_DUPLEX, "));
        }

        ET_LOG1(ETH.linkSpeed(), F("Mbps"));

        ESP32_W5500_eth_connected = true;
      }

      break;

    case ARDUINO_EVENT_ETH_DISCONNECTED:
      ET_LOG("\nETH Disconnected");
      ESP32_W5500_eth_connected = false;
      break;

    case ARDUINO_EVENT_ETH_STOP:
      ET_LOG("\nETH Stopped");
      ESP32_W5500_eth_connected = false;
      break;

#else

    // For old core v1.0.6-
    // Core v2.0.0 defines a stupid enum arduino_event_id_t, breaking any code for ESP32_W5500 written for previous core
    // Why so strange to define a breaking enum arduino_event_id_t in WiFiGeneric.h
    // compared to the old system_event_id_t, now in tools/sdk/esp32/include/esp_event/include/esp_event_legacy.h
    // You can preserve the old enum order and just adding new items to do no harm
    case SYSTEM_EVENT_ETH_START:
      ET_LOG(F("\nETH Started"));
      //set eth hostname here
      ETH.setHostname("ESP32_W5500");
      break;

    case SYSTEM_EVENT_ETH_CONNECTED:
      ET_LOG(F("ETH Connected"));
      break;

    case SYSTEM_EVENT_ETH_GOT_IP:
      if (!ESP32_W5500_eth_connected)
      {
        ET_LOG3(F("ETH MAC: "), ETH.macAddress(), F(", IPv4: "), ETH.localIP());

        if (ETH.fullDuplex())
        {
          ET_LOG0(F("FULL_DUPLEX, "));
        }
        else
        {
          ET_LOG0(F("HALF_DUPLEX, "));
        }

        ET_LOG1(ETH.linkSpeed(), F("Mbps"));

        ESP32_W5500_eth_connected = true;
      }

      break;

    case SYSTEM_EVENT_ETH_DISCONNECTED:
      ET_LOG("ETH Disconnected");
      ESP32_W5500_eth_connected = false;
      break;

    case SYSTEM_EVENT_ETH_STOP:
      ET_LOG("\nETH Stopped");
      ESP32_W5500_eth_connected = false;
      break;
#endif

    default:
      break;
  }
}
