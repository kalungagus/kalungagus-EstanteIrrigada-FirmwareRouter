//==================================================================================================
//                           MÓDULO DE GERENCIAMENTO DA CONEXÃO WIFI
//==================================================================================================
#ifndef _WIFI_MANAGER_
#define _WIFI_MANAGER_

#include "MessageManager.h"

//==================================================================================================
// Funções
//==================================================================================================
extern void initWiFiManager(commInterface_t *manager);
extern void connectToWiFi(void);
extern bool isWiFiConnected(void);
extern bool isFirebaseReady(void);
extern void resetWiFiConnection(void);
extern bool isClientConnected(void);
extern bool sendDataToDatabase(char *packet);

#endif
//==================================================================================================
