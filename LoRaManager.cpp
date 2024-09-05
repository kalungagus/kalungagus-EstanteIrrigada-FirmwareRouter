//==================================================================================================
// Includes
//==================================================================================================
#include "SystemDefinitions.h"
#include "SerialManager.h"
#include "System.h"
#include <SPI.h>
#include <LoRa.h>

//==================================================================================================
// Variáveis do módulo
//==================================================================================================
QueueHandle_t messageLoRaQueue;
serialBuffer loraSerialBuffer;
SemaphoreHandle_t loraSemaphore = xSemaphoreCreateMutex();
xTaskHandle taskLoRaReceiveHandle;

// Definicacao de constantes
const int csPin = 5;           // Chip Select (Slave Select do protocolo SPI) do modulo Lora
const int resetPin = 15;       // Reset do modulo LoRa
const int irqPin = 21;         // Pino DI0

//==================================================================================================
// Funções do módulo, declaradas aqui para melhor organização do arquivo
//==================================================================================================
static void taskLoRaReceive(void *);
static void taskLoRaSend(void *);

//==================================================================================================
// Vetores de interrupção
//==================================================================================================
void onLoRaReceive(int packetSize)
{
  BaseType_t xYieldRequired;
  xYieldRequired = xTaskResumeFromISR(taskLoRaReceiveHandle);
  portYIELD_FROM_ISR(xYieldRequired);
}

//==================================================================================================
// Funções
//==================================================================================================
void initLoRaManager(void)
{
  memset(&loraSerialBuffer, 0, sizeof(loraSerialBuffer));
  
  LoRa.setPins(csPin, resetPin, irqPin);

  if (!LoRa.begin(433E6)) 
  {
    sendMessageWithNewLine("Erro ao iniciar modulo LoRa. Verifique a coenxao dos seus pinos!! ", PRIORITY_SELECT);
    while (true);
  }
  sendMessageWithNewLine("Modulo LoRa iniciado com sucesso!!! :) ", PRIORITY_SELECT);
  messageLoRaQueue = xQueueCreate(MESSAGE_QUEUE_SIZE, MAX_PACKET_SIZE);
  xTaskCreate(taskLoRaSend, "LoRaSend", 8192, NULL, 2, NULL);
  xTaskCreate(taskLoRaReceive, "LoRaEvent", 8192, NULL, 2, &taskLoRaReceiveHandle);
  vTaskSuspend(taskLoRaReceiveHandle);
  LoRa.onReceive(onLoRaReceive);
  LoRa.receive();
}

void sendRawLoRaBuffer(char *buff)
{
  xQueueSend(messageLoRaQueue, (void *)buff, (TickType_t)0);
}

static void taskLoRaReceive(void *pvParameters)
{
  for(;;)
  {
    if(xSemaphoreTake(loraSemaphore, (TickType_t )portMAX_DELAY) == pdTRUE)
    {
      while (LoRa.available())
        processLoRaCharReception((char)LoRa.read(), &loraSerialBuffer);

      xSemaphoreGive(loraSemaphore);
      vTaskSuspend(NULL);   // A task se suspende
    }
  }
}

static void taskLoRaSend(void *pvParameters)
{
  char txPacket[MAX_PACKET_SIZE];

  for(;;)
  {
    if(xQueueReceive(messageLoRaQueue, &txPacket, (TickType_t)portMAX_DELAY) == pdPASS)
    {
      if(xSemaphoreTake(loraSemaphore, (TickType_t)portMAX_DELAY) == pdTRUE)
      {
        uint8_t length = txPacket[2] + 3;

        LoRa.beginPacket();
        for(uint8_t index = 0; index < length; index++)
          LoRa.write(txPacket[index]);
        LoRa.endPacket();
      }
      LoRa.receive();
      xSemaphoreGive(loraSemaphore);
    }
  }
}

//==================================================================================================