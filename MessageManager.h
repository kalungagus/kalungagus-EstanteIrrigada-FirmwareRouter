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
#define DIRECT_TO_LORA             2
#define MAX_PACKET_SIZE            50
#define MESSAGE_QUEUE_SIZE         5

//==================================================================================================
// Máscaras de comandos
//==================================================================================================
#define BROAD_COMMAND             0xC0
#define ENDPOINT_COMMAND          0x80
#define ROUTER_COMMAND            0x40
#define COMMAND_SOURCE_MODULE     0x20
#define COMMAND_SOURCE_SOFTWARE   0x10
#define COMMAND_SOURCE_ROUTER     0x00
#define COMMAND_MASK              0x0F
#define SOURCE_MASK               0x30

//==================================================================================================
// Comandos do módulo
//==================================================================================================
#define CMD_MESSAGE               0x00
#define CMD_GET_DATETIME          0x01
#define CMD_SET_DATETIME          0x02
#define CMD_SEND_SAMPLES          0x03
#define CMD_GET_CONTROL_CONFIG    0x04
#define CMD_SET_CONTROL_CONFIG    0x05
#define CMD_SAVE_CONFIG           0x06
#define CMD_POWER_DOWN            0x07
#define CMD_REQUEST_ACTION        0x08
#define CMD_SET_TIMEOUT           0x09

//==================================================================================================
// Tipos de dados padrão
//==================================================================================================
typedef struct
{
  unsigned char state;
  unsigned char messageSize;
  unsigned char bytesReaded;
  unsigned char originMask;
  QueueHandle_t transmissionQueue;
  QueueHandle_t forwardQueue;
  char packet[MAX_PACKET_SIZE];
} commInterface_t;

//==================================================================================================
// Funções
//==================================================================================================
extern void initMessageManager(void);
extern uint8_t intToBcd(uint16_t data);
extern uint16_t bcdToInt(unsigned char data);
extern void processCharReception(unsigned char data, commInterface_t *thisCommManager);
extern void sendCmdString(unsigned char, String, unsigned int destiny);
extern void sendMessage(String, unsigned int destiny);
extern void sendMessageWithNewLine(String, unsigned int destiny);
extern void sendCmdBuff(unsigned char cmd, char *buff, unsigned int length, unsigned int destiny);
extern void sendCmd(unsigned char cmd, unsigned int destiny);
extern void sendAck(unsigned char cmd, unsigned int destiny);
extern void loadIntoArray(uint32_t value, char *buffer);

#endif
