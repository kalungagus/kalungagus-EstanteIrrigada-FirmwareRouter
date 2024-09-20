#include "MessageManager.h"
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_task_wdt.h"
#include "rtc_wdt.h"

void setup() 
{
  esp_task_wdt_config_t twdt_config = 
  {
    .timeout_ms = 20000,
    .idle_core_mask = 1,
    .trigger_panic = false,
  };
  
  esp_task_wdt_init(&twdt_config);
  rtc_wdt_set_time(RTC_WDT_STAGE0, 20000);

  initMessageManager();
  sendMessageWithNewLine("Roteador LoRa - WiFi", PRIORITY_SELECT);
  sendMessageWithNewLine("Inicializacao completa.", PRIORITY_SELECT);
}

void loop() 
{
  delay(1000);
}