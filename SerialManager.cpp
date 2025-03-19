//==================================================================================================
// Includes
//==================================================================================================
#include "SystemDefinitions.h"
#include "MessageManager.h"

//==================================================================================================
// Variáveis do módulo
//==================================================================================================
static SemaphoreHandle_t serialSemaphore = xSemaphoreCreateMutex();
static xTaskHandle taskSerialReceiveHandle;

//==================================================================================================
// Vetores de interrupção
//==================================================================================================
void onSerialReceive(void)
{
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  vTaskNotifyGiveFromISR(taskSerialReceiveHandle, &xHigherPriorityTaskWoken);
  portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

//==================================================================================================
// Funções
//==================================================================================================
static void taskSerialReceive(void *pvParameters)
{
  commInterface_t *manager = (commInterface_t *)pvParameters;
  uint32_t ulNotifiedValue;

  for(;;)
  {
    ulNotifiedValue = ulTaskNotifyTakeIndexed(0, pdTRUE, (TickType_t )portMAX_DELAY);

    if(ulNotifiedValue > 0)
    {
      if(xSemaphoreTake(serialSemaphore, (TickType_t)portMAX_DELAY) == pdTRUE)
      {
        while(Serial.available())
        {
          processCharReception((char)Serial.read(), manager);
          vTaskDelay( 1 );  // Delay rápido para o FreeRTOS atender outras tasks
        }

        xSemaphoreGive(serialSemaphore);
      }
    }
  }
}

static void taskSerialSend(void *pvParameters)
{
  char txPacket[MAX_PACKET_SIZE];
  commInterface_t *manager = (commInterface_t *)pvParameters;

  for(;;)
  {
    if(xQueueReceive(manager->transmissionQueue, &txPacket, (TickType_t)portMAX_DELAY) == pdPASS)
    {
      if(xSemaphoreTake(serialSemaphore, (TickType_t)portMAX_DELAY) == pdTRUE)
      {
        int length = txPacket[2] + 3;
        
        Serial.write(txPacket, length);
        Serial.flush();
        xSemaphoreGive(serialSemaphore);
      }
    }
  }
}

void initSerialManager(commInterface_t *manager)
{
  Serial.begin(115200);
  xTaskCreate(taskSerialSend, "SerialSend", 2000, manager, 2, NULL);
  xTaskCreatePinnedToCore(taskSerialReceive, "SerialEvent", 2000, manager, 3, &taskSerialReceiveHandle, 0);
  Serial.onReceive(onSerialReceive);
}

//==================================================================================================
