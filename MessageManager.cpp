//==================================================================================================
// Includes
//==================================================================================================
#include "SystemDefinitions.h"
#include "WiFiManager.h"
#include "SerialManager.h"
#include "LoRaManager.h"

commInterface_t serialComm, wifiComm, loraComm;
QueueHandle_t selectQueue;

//==================================================================================================
// Funções
//==================================================================================================
static void taskSelectQueue(void *pvParameters)
{
  char txBuffer[MAX_PACKET_SIZE];

  for(;;)
  {
    if(xQueueReceive(selectQueue, &txBuffer, (TickType_t)portMAX_DELAY) == pdPASS)
    {
      xQueueSend(wifiComm.transmissionQueue, (void *)txBuffer, (TickType_t)0);
      xQueueSend(serialComm.transmissionQueue, (void *)txBuffer, (TickType_t)0);
    }
  }
}

void initMessageManager(void)
{
  memset(&serialComm, 0, sizeof(serialComm));
  memset(&wifiComm, 0, sizeof(wifiComm));
  memset(&loraComm, 0, sizeof(loraComm));

  serialComm.transmissionQueue = xQueueCreate(MESSAGE_QUEUE_SIZE, MAX_PACKET_SIZE);
  wifiComm.transmissionQueue = xQueueCreate(MESSAGE_QUEUE_SIZE, MAX_PACKET_SIZE);
  loraComm.transmissionQueue = xQueueCreate(MESSAGE_QUEUE_SIZE, MAX_PACKET_SIZE);
  selectQueue = xQueueCreate(MESSAGE_QUEUE_SIZE, MAX_PACKET_SIZE);
  serialComm.forwardQueue = loraComm.transmissionQueue;
  wifiComm.forwardQueue = loraComm.transmissionQueue;
  loraComm.forwardQueue = selectQueue;

  initSerialManager(&serialComm);
  initWiFiManager(&wifiComm);
  initLoRaManager(&loraComm);

  xTaskCreate(taskSelectQueue, "taskSelectQueue", 2000, NULL, 2, NULL);
}

void processCharReception(unsigned char data, commInterface_t *thisCommManager)
{
  switch(thisCommManager->state)
  {
    case 0:
      if(data == 0xAA)
      {
        thisCommManager->bytesReaded=0;
        thisCommManager->packet[thisCommManager->bytesReaded++] = data;
        thisCommManager->state++;
      }
      break;
    case 1:
      if(data == 0x55)
      {
        thisCommManager->packet[thisCommManager->bytesReaded++] = data;
        thisCommManager->state++;
      }
      else
        thisCommManager->state = 0;
      break;
    case 2:
      thisCommManager->messageSize = ((data+3) > MAX_PACKET_SIZE) ? MAX_PACKET_SIZE : data+3;
      thisCommManager->packet[thisCommManager->bytesReaded++] = data;
      thisCommManager->state++;
      break;
    default:
      if(thisCommManager->bytesReaded < thisCommManager->messageSize)
      {
        thisCommManager->packet[thisCommManager->bytesReaded++] = data;
        if(thisCommManager->bytesReaded >= thisCommManager->messageSize)
        {
          xQueueSend(thisCommManager->forwardQueue, (void *)thisCommManager->packet, (TickType_t)0);
          thisCommManager->state = 0;
        }
      }
      else
        thisCommManager->state = 0;
      break;
  }
}

void sendCmdString(unsigned char cmd, String s, unsigned int isDirect)
{
  unsigned char txBuffer[MAX_PACKET_SIZE];
  const char *charArray = s.c_str();
  unsigned int length = s.length();

  length = (length+4 > MAX_PACKET_SIZE) ? MAX_PACKET_SIZE - 4 : length;
  txBuffer[0] = 0xAA;
  txBuffer[1] = 0x55;
  txBuffer[2] = length + 1;
  txBuffer[3] = cmd;

  for(int i=0; i<length; i++)
    txBuffer[4+i] = charArray[i];

  if(isDirect == PRIORITY_SELECT && isClientConnected())
    xQueueSend(wifiComm.transmissionQueue, (void *)txBuffer, (TickType_t)0);
  else
    xQueueSend(serialComm.transmissionQueue, (void *)txBuffer, (TickType_t)0);
}

void sendMessage(String s, unsigned int isDirect)
{
  sendCmdString(CMD_MESSAGE, s, isDirect);
}

void sendMessageWithNewLine(String s, unsigned int isDirect)
{
  String msg = s + "\r\n";
  sendCmdString(CMD_MESSAGE, msg, isDirect);
}

void sendCmdBuff(unsigned char cmd, char *buff, unsigned int length, unsigned int isDirect)
{
  unsigned char txBuffer[MAX_PACKET_SIZE];

  length = (length+4 > MAX_PACKET_SIZE) ? MAX_PACKET_SIZE - 4 : length;
  txBuffer[0] = 0xAA;
  txBuffer[1] = 0x55;
  txBuffer[2] = length + 1;
  txBuffer[3] = cmd;

  for(int i=0; i<length; i++)
    txBuffer[4+i] = buff[i];

  if(isDirect == PRIORITY_SELECT && isClientConnected())
    xQueueSend(wifiComm.transmissionQueue, (void *)txBuffer, (TickType_t)0);
  else
    xQueueSend(serialComm.transmissionQueue, (void *)txBuffer, (TickType_t)0);
}

void sendAck(unsigned char cmd, unsigned int isDirect)
{
  unsigned char txBuffer[4] = {0xAA, 0x55, 0x01, 0x06};

  if(isDirect == PRIORITY_SELECT && isClientConnected())
    xQueueSend(wifiComm.transmissionQueue, (void *)txBuffer, (TickType_t)0);
  else
    xQueueSend(serialComm.transmissionQueue, (void *)txBuffer, (TickType_t)0);
}

void loadIntoArray(uint32_t value, char *buffer)
{
  for(int i=0; i<sizeof(uint32_t); i++)
    buffer[i] = ((char *)(&value))[i];
}

//==================================================================================================
