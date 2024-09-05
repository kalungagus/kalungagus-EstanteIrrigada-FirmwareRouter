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

//==================================================================================================
// Tipos de dados padrão
//==================================================================================================
typedef struct
{
  unsigned char state;
  unsigned char messageSize;
  unsigned char bytesReaded;
  char packet[MAX_PACKET_SIZE];
} serialBuffer;

//==================================================================================================
// Funções
//==================================================================================================
extern void initMessageManager(void);
extern void sendMessage(String, unsigned int isDirect);
extern void sendMessageWithNewLine(String, unsigned int isDirect);
extern void sendCmdString(unsigned char, String, unsigned int isDirect);
extern void sendRawBuffer(char *buff, unsigned int isDirect);
extern void sendAck(unsigned char cmd, unsigned int isDirect);
extern void sendCmdBuff(unsigned char cmd, char *buff, unsigned int length, unsigned int isDirect);

#endif
