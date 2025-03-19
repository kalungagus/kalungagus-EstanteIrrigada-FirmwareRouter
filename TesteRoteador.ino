#include "MessageManager.h"
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_task_wdt.h"
#include "rtc_wdt.h"

void setup() 
{
  String wtdMessage;
  esp_task_wdt_config_t twdt_config = 
  {
    .timeout_ms = 20000,
    .idle_core_mask = 1,
    .trigger_panic = false,
  };

  #if !CONFIG_ESP_TASK_WDT_INIT
  if(esp_task_wdt_init(&twdt_config) == ESP_OK)
  #else
  if(esp_task_wdt_reconfigure(&twdt_config) == ESP_OK)
  #endif
    wtdMessage = "TaskWatchDog Inicializado";
  else
    wtdMessage = "TaskWatchDog nao inicializado";

  initMessageManager();
  sendMessageWithNewLine("Roteador LoRa - WiFi", PRIORITY_SELECT);
  sendMessageWithNewLine("Inicializacao completa.", PRIORITY_SELECT);
  sendMessageWithNewLine(wtdMessage, PRIORITY_SELECT);
}

void loop() 
{
  delay(1000);
}