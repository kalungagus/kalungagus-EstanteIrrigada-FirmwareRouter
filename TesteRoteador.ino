#include "MessageManager.h"

void setup() 
{
  initMessageManager();
  sendMessageWithNewLine("Roteador LoRa - WiFi", PRIORITY_SELECT);
  sendMessageWithNewLine("Inicializacao completa.", PRIORITY_SELECT);
}

void loop() 
{
  delay(1000);
}


