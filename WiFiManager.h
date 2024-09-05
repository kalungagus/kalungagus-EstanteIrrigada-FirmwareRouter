//==================================================================================================
//                           MÓDULO DE GERENCIAMENTO DA CONEXÃO WIFI
//==================================================================================================
#ifndef _WIFI_MANAGER_
#define _WIFI_MANAGER_

//==================================================================================================
// Funções
//==================================================================================================
extern void initWiFiManager(uint8_t wifiState);
extern void connectToWiFi(void);
extern bool isWiFiConnected(void);
extern void resetWiFiConnection(void);
extern bool isClientConnected(void);

extern void setWifiOn(void);
extern void setWiFiOff(void);

//==================================================================================================
// Variáveis
//==================================================================================================
extern QueueHandle_t transmissionQueue;

#endif
//==================================================================================================
