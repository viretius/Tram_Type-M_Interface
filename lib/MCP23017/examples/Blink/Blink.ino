#include <Wire.h>
#include <MCP23017.h>

#define CHECK_BIT(var, pos) ((var >> pos) & 1)

MCP23017 mcp_list[4];
uint16_t portMode = 0b1000111110001111; // Binary representation of 1000 1111 1000 1111

unsigned long previousMillis = 0;  // will store last time LED was updated

// constants won't change:
const long interval = 1000;

int i, t, j, state = 0;

void setup() {
  Serial.begin(9600);
  Serial.println();

  // Print the port mode in correct bit order
  Serial.print("Port mode: ");
  for (int j = 15; j >= 0; j--) {  
    Serial.print(CHECK_BIT(portMode, j));
  }

  Serial.println("\nPortA (MSB): ");
  for (int j = 7; j >= 0; j--) {
    Serial.print(CHECK_BIT((portMode >> 8), j));
  }

  Serial.println("\nPortB (LSB): ");
  for (int j = 7; j >= 0; j--) {
    Serial.print(CHECK_BIT(portMode, j));
  }

  for (int i = 0; i < 4; i++) 
  {
    mcp_list[i] = MCP23017();
      
    if (!mcp_list[i].init(i+32)) {
      Serial.printf("\n\nMCP23017 mit der Adresse 0x%x konnte nicht initialisiert werden.", i+32);
      return;
    }
    
    mcp_list[i].portMode(MCP23017Port::A, (portMode >> 8) & 0xFF); //input_pullups enabled by default 
    mcp_list[i].portMode(MCP23017Port::B, portMode & 0xFF);
    mcp_list[i].writeRegister(MCP23017Register::GPIO_A, 0x00);
    mcp_list[i].writeRegister(MCP23017Register::GPIO_B, 0x00);  //Reset ports
    mcp_list[i].writeRegister(MCP23017Register::IPOL_A, 0x00);    
    mcp_list[i].writeRegister(MCP23017Register::IPOL_B, 0x00);  //If a bit is set, the corresponding GPIO register bit will reflect the inverted value on the pin                                                               
  }

}


void blink() {

  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
      for (uint8_t i = 0; i < 4; i++) 
      {
        for (int8_t t = 0; t < 16; t++) 
        {
          if (CHECK_BIT(portMode, t)) continue; // only set outputs 
          mcp_list[i].digitalWrite(t, !state);
        }
      }
      state = !state;
  }
}

void loop() {

  blink();
}