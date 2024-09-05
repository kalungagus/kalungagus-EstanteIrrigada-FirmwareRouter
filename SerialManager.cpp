//==================================================================================================
// Includes
//==================================================================================================
#include "SystemDefinitions.h"
#include "WiFiManager.h"
#include "System.h"

//==================================================================================================
// Macros
//==================================================================================================
#define DEBUG()           digitalWrite(DEBUG_PIN, !digitalRead(DEBUG_PIN))

//==================================================================================================
// QUEUE SELECT
//==================================================================================================
#define PRIORITY_SELECT            0
#define DIRECT_TO_SERIAL           1

//==================================================================================================
// Funções do módulo, declaradas aqui para melhor organização do arquivo
//==================================================================================================
void initSerialManager(void);
void sendMessage(String, unsigned int);
void sendMessageWithNewLine(String, unsigned int);
void sendCmdBuff(unsigned char, char *, unsigned int, unsigned int);
void sendCmdString(unsigned char, String, unsigned int);
void sendAck(unsigned char, unsigned int);
static void taskSerialReceive(void *);
static void taskSerialSend(void *);
static void loadIntoArray(uint32_t, char *);

// Esta função deve ser declarada em um arquivo de projeto para tratar os comandos recebidos por este
// módulo
extern void processReception(char *packet, unsigned int packetSize);

//==================================================================================================
// Variáveis do módulo
//==================================================================================================
QueueHandle_t messageQueue;
QueueHandle_t directSerialQueue;
serialBuffer usbReceivedData;
SemaphoreHandle_t serialSemaphore = xSemaphoreCreateMutex();
xTaskHandle taskSerialReceiveHandle;

//==================================================================================================
// Vetores de interrupção
//==================================================================================================
void onSerialReceive(void)
{
  BaseType_t xYieldRequired;
  xYieldRequired = xTaskResumeFromISR(taskSerialReceiveHandle);
  portYIELD_FROM_ISR(xYieldRequired);
}

//==================================================================================================
// Funções
//==================================================================================================
void initSerialManager(void)
{
  memset(&usbReceivedData, 0, sizeof(usbReceivedData));
  
  Serial.begin(115200);
  messageQueue = xQueueCreate(MESSAGE_QUEUE_SIZE, MAX_PACKET_SIZE);
  directSerialQueue = xQueueCreate(MESSAGE_QUEUE_SIZE, MAX_PACKET_SIZE);
  xTaskCreate(taskSerialSend, "SerialSend", 2000, NULL, 2, NULL);
  xTaskCreate(taskSerialReceive, "SerialEvent", 2000, NULL, 2, &taskSerialReceiveHandle);
  vTaskSuspend(taskSerialReceiveHandle);
  Serial.onReceive(onSerialReceive);
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

  if(isDirect == PRIORITY_SELECT)
    xQueueSend(messageQueue, (void *)txBuffer, (TickType_t)0);
  else
    xQueueSend(directSerialQueue, (void *)txBuffer, (TickType_t)0);
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

static void taskSerialReceive(void *pvParameters)
{
  char CharSerialRX;
  unsigned char messageSize=0;
  int serialState = 0;
  int size=0;

  for(;;)
  {
    if(xSemaphoreTake(serialSemaphore, (TickType_t)portMAX_DELAY) == pdTRUE)
    {
      while(Serial.available())
      {
        CharSerialRX = (char)Serial.read();
        processCharReception(CharSerialRX, &usbReceivedData);
      }

      xSemaphoreGive(serialSemaphore);
      vTaskSuspend(NULL);   // A task se suspende
    }
  }
}

static void taskSerialSend(void *pvParameters)
{
  char txPacket[MAX_PACKET_SIZE];
  bool waitPackets;
  
  for(;;)
  {
    waitPackets = true;
    
    if(xSemaphoreTake(serialSemaphore, (TickType_t)0) == pdTRUE)
    {
      if(isClientConnected() == false)
      {
        if(xQueueReceive(messageQueue, &txPacket, (TickType_t)0) == pdPASS)
        {
          int length = txPacket[2] + 3;
          
          Serial.write(txPacket, length);
          Serial.flush();
          waitPackets = false;
        }
      }
      
      if(xQueueReceive(directSerialQueue, &txPacket, (TickType_t)0) == pdPASS)
      {
        int length = txPacket[2] + 3;
        
        Serial.write(txPacket, length);
        Serial.flush();
        waitPackets = false;
      }

      xSemaphoreGive(serialSemaphore);
    }

    if(waitPackets)
      vTaskDelay( 10 / portTICK_PERIOD_MS );
  }
}

//==================================================================================================
