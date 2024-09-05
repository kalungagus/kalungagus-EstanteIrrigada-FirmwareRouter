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
// Comandos
//==================================================================================================
#define CMD_MESSAGE              0x40

//==================================================================================================
// Configurações
//==================================================================================================
#define SERIAL_SEND_HEADER            1
#define MAX_PACKET_SIZE               50
#define MESSAGE_QUEUE_SIZE            10
#define MAX_CONNECTION_IDLE_STATUS    5
#define MAX_DISCONNECTED_MESSAGES     10

#define DEBUG_PIN                     26
#define VALVE_PIN                     15

//--------------------------------------------------------------------------------------------------
// Sensores
//--------------------------------------------------------------------------------------------------
#define SENS_1                ADC1_CHANNEL_0
#define SENS_2                ADC1_CHANNEL_3
#define SENS_3                ADC1_CHANNEL_6
#define SENS_4                ADC1_CHANNEL_7
#define SENS_5                ADC1_CHANNEL_4
#define SENS_6                ADC1_CHANNEL_5
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

//==================================================================================================
// Tipos de dados padrão para todo o projeto
//==================================================================================================
typedef struct
{
  unsigned char state;
  unsigned char messageSize;
  unsigned char bytesReaded;
  char packet[MAX_PACKET_SIZE];
} serialBuffer;

#endif
