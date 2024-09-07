//**************************************************************************************************
//                                    Definições do sistema atual
// Descrição: Aqui está organizado o sistema de controle para os projetos
//**************************************************************************************************
#ifndef _SYSTEM_DEFINITIONS_
#define _SYSTEM_DEFINITIONS_

//==================================================================================================
// Includes
//==================================================================================================
#include <Arduino.h>

//==================================================================================================
// Configurações
//==================================================================================================
#define MAX_CONNECTION_IDLE_STATUS    5
#define MAX_DISCONNECTED_MESSAGES     10

//--------------------------------------------------------------------------------------------------
// Parâmetros de SSID e Senha de rede
//--------------------------------------------------------------------------------------------------
#define SSID_ADDRESS                 0
#define SSID_LENGHT                  20
#define PASSWORD_ADDRESS             SSID_ADDRESS+SSID_LENGHT
#define PASSWORD_LENGTH              20
#define BUTTON_SAMPLES_ADDRESS       PASSWORD_ADDRESS+PASSWORD_LENGTH
#define BUTTON_SAMPLES_LENGHT        2
#define WIFI_ENABLE_ADDRESS          BUTTON_SAMPLES_ADDRESS+BUTTON_SAMPLES_LENGHT
#define WIFI_ENABLE_LENGHT           1
#define EEPROM_SIZE                  BUTTON_SAMPLES_ADDRESS+BUTTON_SAMPLES_LENGHT+WIFI_ENABLE_LENGHT

//--------------------------------------------------------------------------------------------------
// Credenciais de rede
//--------------------------------------------------------------------------------------------------
#include "Security.h"

#endif
