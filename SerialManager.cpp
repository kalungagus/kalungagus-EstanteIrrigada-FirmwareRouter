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
  BaseType_t xYieldRequired;
  xYieldRequired = xTaskResumeFromISR(taskSerialReceiveHandle);
  portYIELD_FROM_ISR(xYieldRequired);
}

//==================================================================================================
// Funções
//==================================================================================================
static void taskSerialReceive(void *pvParameters)
{
  commInterface_t *manager = (commInterface_t *)pvParameters;

  for(;;)
  {
    if(xSemaphoreTake(serialSemaphore, (TickType_t)portMAX_DELAY) == pdTRUE)
    {
      while(Serial.available())
        processCharReception((char)Serial.read(), manager);

      xSemaphoreGive(serialSemaphore);
      vTaskSuspend(NULL);   // A task se suspende
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
  xTaskCreate(taskSerialReceive, "SerialEvent", 2000, manager, 2, &taskSerialReceiveHandle);
  vTaskSuspend(taskSerialReceiveHandle);
  Serial.onReceive(onSerialReceive);
}

//==================================================================================================
