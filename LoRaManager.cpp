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
// Baseado em https://www.freertos.org/Documentation/02-Kernel/02-Kernel-features/03-Direct-to-task-notifications/03-As-counting-semaphore
//==================================================================================================
void onLoRaReceive(int packetSize)
{
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  vTaskNotifyGiveFromISR(taskLoRaReceiveHandle, &xHigherPriorityTaskWoken);
  portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

//==================================================================================================
// Funções
//==================================================================================================
static void taskLoRaReceive(void *pvParameters)
{
  commInterface_t *manager = (commInterface_t *)pvParameters;
  uint32_t ulNotifiedValue;

  for(;;)
  {
    ulNotifiedValue = ulTaskNotifyTakeIndexed(0, pdTRUE, (TickType_t )portMAX_DELAY);

    if(ulNotifiedValue > 0)
    {
      if(xSemaphoreTake(loraSemaphore, (TickType_t )portMAX_DELAY) == pdTRUE)
      {
        while (LoRa.available())
        {
          processCharReception((char)LoRa.read(), manager);
          vTaskDelay( 1 );  // Delay rápido para o FreeRTOS atender outras tasks
        }

        xSemaphoreGive(loraSemaphore);
      }
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
        {
          LoRa.write(txPacket[index]);
          vTaskDelay( 1 );  // Delay rápido para o FreeRTOS atender outras tasks
        }

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
  xTaskCreatePinnedToCore(taskLoRaReceive, "LoRaEvent", 8192, manager, 4, &taskLoRaReceiveHandle, 0);
  LoRa.onReceive(onLoRaReceive);
  LoRa.receive();
}

//==================================================================================================