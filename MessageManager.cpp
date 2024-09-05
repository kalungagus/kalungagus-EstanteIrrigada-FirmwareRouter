//==================================================================================================
// Includes
//==================================================================================================
#include "SystemDefinitions.h"
#include "WiFiManager.h"
#include "System.h"

// Esta função deve ser declarada em um arquivo de projeto para tratar os comandos recebidos por este
// módulo
extern void processReception(char *packet, unsigned int packetSize);

QueueHandle_t *serialQueue, *wifiQueue;

//==================================================================================================
// Funções
//==================================================================================================
void initMessageManager(void)
{
  serialQueue = initSerialManager();
  initWiFiManager(1);
}

void switchMessageToQueue(unsigned int isDirect)
{
  if(isDirect == PRIORITY_SELECT && isClientConnected())
    xQueueSend(messageQueue, (void *)txBuffer, (TickType_t)0);
  else
    xQueueSend(directSerialQueue, (void *)txBuffer, (TickType_t)0);
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

  if(isDirect == PRIORITY_SELECT)
    xQueueSend(messageQueue, (void *)txBuffer, (TickType_t)0);
  else
    xQueueSend(directSerialQueue, (void *)txBuffer, (TickType_t)0);
}

void sendRawBuffer(char *buff, unsigned int isDirect)
{
  if(isDirect == PRIORITY_SELECT)
    xQueueSend(messageQueue, (void *)buff, (TickType_t)0);
  else
    xQueueSend(directSerialQueue, (void *)buff, (TickType_t)0);
}

void sendAck(unsigned char cmd, unsigned int isDirect)
{
  #if (SERIAL_SEND_HEADER == 1)
  unsigned char txBuffer[4] = {0xAA, 0x55, 0x01, 0x06};
  #else
  unsigned char txBuffer[4] = {'O', 'K', '\r', '\n'};
  #endif

  if(isDirect == PRIORITY_SELECT)
    xQueueSend(messageQueue, (void *)txBuffer, (TickType_t)0);
  else
    xQueueSend(directSerialQueue, (void *)txBuffer, (TickType_t)0);
}

static void loadIntoArray(uint32_t value, char *buffer)
{
  for(int i=0; i<sizeof(uint32_t); i++)
    buffer[i] = ((char *)(&value))[i];
}

//==================================================================================================
