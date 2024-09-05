//==================================================================================================
// Includes
//==================================================================================================
#include <SPI.h>
#include <LoRa.h>
#include "SystemDefinitions.h"
#include "MessageManager.h"

//==================================================================================================
// Variáveis do módulo
//==================================================================================================
SemaphoreHandle_t loraSemaphore = xSemaphoreCreateMutex();
xTaskHandle taskLoRaReceiveHandle;

// Definicacao de constantes
const int csPin = 5;           // Chip Select (Slave Select do protocolo SPI) do modulo Lora
const int resetPin = 15;       // Reset do modulo LoRa
const int irqPin = 21;         // Pino DI0

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
static void taskLoRaReceive(void *pvParameters)
{
  commInterface_t *manager = (commInterface_t *)pvParameters;

  for(;;)
  {
    if(xSemaphoreTake(loraSemaphore, (TickType_t )portMAX_DELAY) == pdTRUE)
    {
      while (LoRa.available())
        processCharReception((char)LoRa.read(), manager);

      xSemaphoreGive(loraSemaphore);
      vTaskSuspend(NULL);   // A task se suspende
    }
  }
}

static void taskLoRaSend(void *pvParameters)
{
  char txPacket[MAX_PACKET_SIZE];
  commInterface_t *manager = (commInterface_t *)pvParameters;

  for(;;)
  {
    if(xQueueReceive(manager->transmissionQueue, &txPacket, (TickType_t)portMAX_DELAY) == pdPASS)
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

void initLoRaManager(commInterface_t *manager)
{  
  LoRa.setPins(csPin, resetPin, irqPin);

  if (!LoRa.begin(433E6)) 
  {
    sendMessageWithNewLine("Erro ao iniciar modulo LoRa. Verifique a coenxao dos seus pinos!! ", PRIORITY_SELECT);
    while (true);
  }
  sendMessageWithNewLine("Modulo LoRa iniciado com sucesso!!! :) ", PRIORITY_SELECT);
  xTaskCreate(taskLoRaSend, "LoRaSend", 8192, manager, 2, NULL);
  xTaskCreate(taskLoRaReceive, "LoRaEvent", 8192, manager, 2, &taskLoRaReceiveHandle);
  vTaskSuspend(taskLoRaReceiveHandle);
  LoRa.onReceive(onLoRaReceive);
  LoRa.receive();
}

//==================================================================================================