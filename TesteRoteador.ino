#include "SystemDefinitions.h"
#include "SerialManager.h"
#include "WiFiManager.h"
#include "LoRaManager.h"
#include "MessageManager.h"
#include "esp_system.h"
#include <soc/sens_reg.h>
#include <soc/sens_struct.h>

void setup() 
{
  initMessageManager();
  sendMessageWithNewLine("Roteador LoRa - WiFi", PRIORITY_SELECT);
  sendMessageWithNewLine("Inicializando...", PRIORITY_SELECT);
  
  initLoRaManager();
  sendMessageWithNewLine("Inicializacao completa.", PRIORITY_SELECT);
}

void processCharReception(unsigned char data, serialBuffer *thisSerial)
{
  switch(thisSerial->state)
  {
    case 0:
      if(data == 0xAA)
      {
        thisSerial->bytesReaded=0;
        thisSerial->packet[thisSerial->bytesReaded++] = data;
        thisSerial->state++;
      }
      break;
    case 1:
      if(data == 0x55)
      {
        thisSerial->packet[thisSerial->bytesReaded++] = data;
        thisSerial->state++;
      }
      else
        thisSerial->state = 0;
      break;
    case 2:
      thisSerial->messageSize = ((data+3) > MAX_PACKET_SIZE) ? MAX_PACKET_SIZE : data+3;
      thisSerial->packet[thisSerial->bytesReaded++] = data;
      thisSerial->state++;
      break;
    default:
      if(thisSerial->bytesReaded < thisSerial->messageSize)
      {
        thisSerial->packet[thisSerial->bytesReaded++] = data;
        if(thisSerial->bytesReaded >= thisSerial->messageSize)
        {
          sendRawLoRaBuffer(thisSerial->packet);
          thisSerial->state = 0;
        }
      }
      else
        thisSerial->state = 0;
      break;
  }
}

void processLoRaCharReception(unsigned char data, serialBuffer *thisSerial)
{
  switch(thisSerial->state)
  {
    case 0:
      if(data == 0xAA)
      {
        thisSerial->bytesReaded=0;
        thisSerial->packet[thisSerial->bytesReaded++] = data;
        thisSerial->state++;
      }
      break;
    case 1:
      if(data == 0x55)
      {
        thisSerial->packet[thisSerial->bytesReaded++] = data;
        thisSerial->state++;
      }
      else
        thisSerial->state = 0;
      break;
    case 2:
      thisSerial->messageSize = ((data+3) > MAX_PACKET_SIZE) ? MAX_PACKET_SIZE : data+3;
      thisSerial->packet[thisSerial->bytesReaded++] = data;
      thisSerial->state++;
      break;
    default:
      if(thisSerial->bytesReaded < thisSerial->messageSize)
      {
        thisSerial->packet[thisSerial->bytesReaded++] = data;
        if(thisSerial->bytesReaded >= thisSerial->messageSize)
        {
          sendRawBuffer(thisSerial->packet, PRIORITY_SELECT);
          thisSerial->state = 0;
        }
      }
      else
        thisSerial->state = 0;
      break;
  }
}

void loop() 
{
  delay(1000);
}


