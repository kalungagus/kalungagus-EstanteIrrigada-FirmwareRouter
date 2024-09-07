//==================================================================================================
//                           MÓDULO DE GERENCIAMENTO DE MENSAGENS
//==================================================================================================
#ifndef _MESSAGE_MANAGER_
#define _MESSAGE_MANAGER_

//==================================================================================================
// QUEUE SELECT
//==================================================================================================
#define PRIORITY_SELECT            0
#define DIRECT_TO_SERIAL           1
#define MAX_PACKET_SIZE            50
#define MESSAGE_QUEUE_SIZE         5

//==================================================================================================
// Comandos do módulo
//==================================================================================================
#define CMD_MESSAGE               0x40
#define CMD_MESSAGE_ECHO          0x80
#define CMD_GET_DATETIME          0x81
#define CMD_SET_DATETIME          0x82
#define CMD_GET_SAMPLES           0x83
#define CMD_GET_CONTROL_CONFIG    0x84
#define CMD_SET_CONTROL_CONFIG    0x85
#define CMD_GET_ALARM_FREQUENCY   0x86
#define CMD_SET_ALARM_FREQUENCY   0x87
#define CMD_SAVE_CONFIG           0x88
#define CMD_POWER_DOWN            0x89
#define CMD_REQUEST_MESSAGE       0x90
#define CMD_HALT_TIMEOUT          0x91


//==================================================================================================
// Tipos de dados padrão
//==================================================================================================
typedef struct
{
  unsigned char state;
  unsigned char messageSize;
  unsigned char bytesReaded;
  QueueHandle_t transmissionQueue;
  QueueHandle_t forwardQueue;
  char packet[MAX_PACKET_SIZE];
} commInterface_t;

//==================================================================================================
// Funções
//==================================================================================================
extern void initMessageManager(void);
extern void processCharReception(unsigned char data, commInterface_t *thisCommManager);
extern void sendCmdString(unsigned char, String, unsigned int isDirect);
extern void sendMessage(String, unsigned int isDirect);
extern void sendMessageWithNewLine(String, unsigned int isDirect);
extern void sendCmdBuff(unsigned char cmd, char *buff, unsigned int length, unsigned int isDirect);
extern void sendAck(unsigned char cmd, unsigned int isDirect);
extern void loadIntoArray(uint32_t value, char *buffer);

#endif
