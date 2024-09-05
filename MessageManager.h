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
