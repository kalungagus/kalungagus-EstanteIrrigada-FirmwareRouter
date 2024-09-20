//==================================================================================================
// Includes
//==================================================================================================
#include "SystemDefinitions.h"
#include "WiFiManager.h"
#include "SerialManager.h"
#include "LoRaManager.h"

//==================================================================================================
// Definições para a máquina de estados do modo de configuração.
// Neste modo, o Software requisita a interrupção do modo power down do módulo, a fim de realizar
// configurações. Durante este modo, o módulo continua enviando requisição de atualizações, que
// pode ser usado para atualizar variáveis baseadas no banco, mas não entra em modo power down
// posteriormente. O roteador volta a definir o módulo em funcionamento normal ou se o software
// requisitar, ou se o software for desconectado.
//==================================================================================================
#define MODULE_NORMAL_OPERATION           0
#define MODULE_REQUEST_CONFIG_MODE        1
#define MODULE_CONFIG_MODE_ON             2

//==================================================================================================
// Macros
//==================================================================================================
#define isAckPacket(c)            (c[2] == 2 && c[4] == 0x06)
#define getPacketOrigin(c)        (c & SOURCE_MASK)

//==================================================================================================
// Variáveis do módulo
//==================================================================================================
commInterface_t serialComm, wifiComm, loraComm;
QueueHandle_t selectQueue, internalCommandQueue;
uint8_t ModuleOperationMode = MODULE_NORMAL_OPERATION, actionPacketIndex=0;

//==================================================================================================
// Funções
//==================================================================================================
static void taskSelectQueue(void *pvParameters)
{
  char txBuffer[MAX_PACKET_SIZE];

  for(;;)
  {
    if(xQueueReceive(selectQueue, &txBuffer, (TickType_t)portMAX_DELAY) == pdPASS)
    {
      xQueueSend(wifiComm.transmissionQueue, (void *)txBuffer, (TickType_t)0);
      xQueueSend(serialComm.transmissionQueue, (void *)txBuffer, (TickType_t)0);
    }
  }
}

uint8_t intToBcd(uint16_t data)
{
  uint8_t value = ((((data & 0x00FF) / 10) << 4) + (data & 0x00FF) % 10);
  return value;
}

uint16_t bcdToInt(unsigned char data)
{
  return ((((data & 0xF0) >> 4) * 10) + (data & 0x0F));
}

void processCharReception(unsigned char data, commInterface_t *thisCommManager)
{
  switch(thisCommManager->state)
  {
    case 0:
      if(data == 0xAA)
      {
        thisCommManager->bytesReaded=0;
        thisCommManager->packet[thisCommManager->bytesReaded++] = data;
        thisCommManager->state++;
      }
      break;
    case 1:
      if(data == 0x55)
      {
        thisCommManager->packet[thisCommManager->bytesReaded++] = data;
        thisCommManager->state++;
      }
      else
        thisCommManager->state = 0;
      break;
    case 2:
      thisCommManager->messageSize = ((data+3) > MAX_PACKET_SIZE) ? MAX_PACKET_SIZE : data+3;
      thisCommManager->packet[thisCommManager->bytesReaded++] = data;
      thisCommManager->state++;
      break;
    case 3:
      data &= ~SOURCE_MASK;
      data |= thisCommManager->originMask;
      thisCommManager->packet[thisCommManager->bytesReaded++] = data;
      if(thisCommManager->bytesReaded >= thisCommManager->messageSize)
      {
        if(thisCommManager->packet[3] & ENDPOINT_COMMAND)
          xQueueSend(thisCommManager->forwardQueue, (void *)thisCommManager->packet, (TickType_t)0);
        if(thisCommManager->packet[3] & ROUTER_COMMAND)
          xQueueSend(internalCommandQueue, (void *)thisCommManager->packet, (TickType_t)0);
        thisCommManager->state = 0;
      }
      else
        thisCommManager->state++;
      break;
    default:
      if(thisCommManager->bytesReaded < thisCommManager->messageSize)
      {
        thisCommManager->packet[thisCommManager->bytesReaded++] = data;
        if(thisCommManager->bytesReaded >= thisCommManager->messageSize)
        {
          if(thisCommManager->packet[3] & ENDPOINT_COMMAND)
            xQueueSend(thisCommManager->forwardQueue, (void *)thisCommManager->packet, (TickType_t)0);
          if(thisCommManager->packet[3] & ROUTER_COMMAND)
            xQueueSend(internalCommandQueue, (void *)thisCommManager->packet, (TickType_t)0);
          thisCommManager->state = 0;
        }
      }
      else
        thisCommManager->state = 0;
      break;
  }
}

void sendCmdString(unsigned char cmd, String s, unsigned int destiny)
{
  unsigned char txBuffer[MAX_PACKET_SIZE];
  const char *charArray = s.c_str();
  unsigned int length = s.length();

  length = (length+4 > MAX_PACKET_SIZE) ? MAX_PACKET_SIZE - 4 : length;
  txBuffer[0] = 0xAA;
  txBuffer[1] = 0x55;
  txBuffer[2] = length + 1;
  txBuffer[3] = cmd;

  for(int i=0; i<length; i++)
    txBuffer[4+i] = charArray[i];

  if(destiny == DIRECT_TO_LORA)
    xQueueSend(loraComm.transmissionQueue, (void *)txBuffer, (TickType_t)0);
  else if(destiny == PRIORITY_SELECT && isClientConnected())
    xQueueSend(wifiComm.transmissionQueue, (void *)txBuffer, (TickType_t)0);
  else
    xQueueSend(serialComm.transmissionQueue, (void *)txBuffer, (TickType_t)0);
}

void sendMessage(String s, unsigned int destiny)
{
  sendCmdString(CMD_MESSAGE, s, destiny);
}

void sendMessageWithNewLine(String s, unsigned int destiny)
{
  String msg = s + "\r\n";
  sendCmdString(CMD_MESSAGE, msg, destiny);
}

void sendCmdBuff(unsigned char cmd, char *buff, unsigned int length, unsigned int destiny)
{
  unsigned char txBuffer[MAX_PACKET_SIZE];

  length = (length+4 > MAX_PACKET_SIZE) ? MAX_PACKET_SIZE - 4 : length;
  txBuffer[0] = 0xAA;
  txBuffer[1] = 0x55;
  txBuffer[2] = length + 1;
  txBuffer[3] = cmd;

  for(int i=0; i<length; i++)
    txBuffer[4+i] = buff[i];

  if(destiny == DIRECT_TO_LORA)
    xQueueSend(loraComm.transmissionQueue, (void *)txBuffer, (TickType_t)0);
  else if(destiny == PRIORITY_SELECT && isClientConnected())
    xQueueSend(wifiComm.transmissionQueue, (void *)txBuffer, (TickType_t)0);
  else
    xQueueSend(serialComm.transmissionQueue, (void *)txBuffer, (TickType_t)0);
}

void sendCmd(unsigned char cmd, unsigned int destiny)
{
  unsigned char txBuffer[MAX_PACKET_SIZE];

  txBuffer[0] = 0xAA;
  txBuffer[1] = 0x55;
  txBuffer[2] = 1;
  txBuffer[3] = cmd;

  if(destiny == DIRECT_TO_LORA)
    xQueueSend(loraComm.transmissionQueue, (void *)txBuffer, (TickType_t)0);
  else if(destiny == PRIORITY_SELECT && isClientConnected())
    xQueueSend(wifiComm.transmissionQueue, (void *)txBuffer, (TickType_t)0);
  else
    xQueueSend(serialComm.transmissionQueue, (void *)txBuffer, (TickType_t)0);
}

void sendAck(unsigned char cmd, unsigned int destiny)
{
  unsigned char txBuffer[5] = {0xAA, 0x55, 0x02, cmd, 0x06};

  if(destiny == DIRECT_TO_LORA)
    xQueueSend(loraComm.transmissionQueue, (void *)txBuffer, (TickType_t)0);
  else if(destiny == PRIORITY_SELECT && isClientConnected())
    xQueueSend(wifiComm.transmissionQueue, (void *)txBuffer, (TickType_t)0);
  else
    xQueueSend(serialComm.transmissionQueue, (void *)txBuffer, (TickType_t)0);
}

void loadIntoArray(uint32_t value, char *buffer)
{
  for(int i=0; i<sizeof(uint32_t); i++)
    buffer[i] = ((char *)(&value))[i];
}

static uint8_t loadDateTimeToBuffer(char *buffer)
{
  uint8_t result = 0;
  struct tm timeinfo;
  
  if (getLocalTime(&timeinfo)) 
  {
    buffer[0] = intToBcd(timeinfo.tm_year - 100);
    buffer[1] = 0;
    buffer[2] = intToBcd(timeinfo.tm_mday);
    buffer[3] = intToBcd(timeinfo.tm_mon + 1);
    buffer[4] = intToBcd(timeinfo.tm_hour);
    buffer[5] = intToBcd(timeinfo.tm_wday);
    buffer[6] = intToBcd(timeinfo.tm_sec);
    buffer[7] = intToBcd(timeinfo.tm_min);
    result = 1;
  }

  return(result);
}

static void getDateTimeFromBuffer(struct tm *timeinfo, char *buffer)
{
  timeinfo->tm_year = bcdToInt(buffer[0]) + 100;
  timeinfo->tm_mday = bcdToInt(buffer[2]);
  timeinfo->tm_mon = bcdToInt(buffer[3]) - 1;
  timeinfo->tm_hour = bcdToInt(buffer[4]);
  timeinfo->tm_wday = bcdToInt(buffer[5]);
  timeinfo->tm_sec = bcdToInt(buffer[6]);
  timeinfo->tm_min = bcdToInt(buffer[7]);
}

static bool isDateTimeUpdated(struct tm *time1)
{
  struct tm timeinfo;
  bool result = false;
  
  if (getLocalTime(&timeinfo)) 
  {
    result = true;
    result &= (time1->tm_year == timeinfo.tm_year);
    result &= (time1->tm_mday == timeinfo.tm_mday);
    result &= (time1->tm_mon == timeinfo.tm_mon);
    result &= (time1->tm_hour == timeinfo.tm_hour);
    result &= (time1->tm_wday == timeinfo.tm_wday);
    result &= (time1->tm_min == timeinfo.tm_min);
  }

  return result;
}

static void sendDateTime(unsigned char origin)
{
  char dateTimeBuff[8];

  if (loadDateTimeToBuffer(dateTimeBuff)) 
  {
    if(origin == COMMAND_SOURCE_MODULE)
      sendCmdBuff(ROUTER_COMMAND | COMMAND_SOURCE_ROUTER | CMD_SET_DATETIME, dateTimeBuff, sizeof(dateTimeBuff), DIRECT_TO_LORA);
    else
      sendCmdBuff(ROUTER_COMMAND | COMMAND_SOURCE_ROUTER | CMD_GET_DATETIME, dateTimeBuff, sizeof(dateTimeBuff), PRIORITY_SELECT);
  }
}

static void processSetTimeout(unsigned char origin, unsigned char response)
{
  char tmpBuffer;

  switch(ModuleOperationMode)
  {
    case MODULE_NORMAL_OPERATION:
      if(origin == COMMAND_SOURCE_SOFTWARE && response != 0)
        ModuleOperationMode = MODULE_REQUEST_CONFIG_MODE;
      break;
    case MODULE_REQUEST_CONFIG_MODE:
      if(origin == COMMAND_SOURCE_SOFTWARE && response == 0)  // Software cancelou o pedido anterior
        ModuleOperationMode = MODULE_NORMAL_OPERATION;
      if(origin == COMMAND_SOURCE_MODULE && response == 0x06)
      {
        ModuleOperationMode = MODULE_CONFIG_MODE_ON;
        sendAck(ROUTER_COMMAND | COMMAND_SOURCE_ROUTER | CMD_SET_TIMEOUT, PRIORITY_SELECT);
      }
      break;
    case MODULE_CONFIG_MODE_ON:
      if(origin == COMMAND_SOURCE_SOFTWARE && response == 0)  // Software saiu do modo de configuração
      {
        ModuleOperationMode = MODULE_NORMAL_OPERATION;
        
        // Reativa o timeout para modo power down
        tmpBuffer = 1;
        sendCmdBuff(ROUTER_COMMAND | CMD_SET_TIMEOUT, &tmpBuffer, 1, DIRECT_TO_LORA);
      }
      break;
    default:
      ModuleOperationMode = MODULE_NORMAL_OPERATION;
      break;
  }
}

static void processModuleActionRequest(struct tm *packetTimeStamp)
{
  char tmpBuffer[8];

  switch(ModuleOperationMode)
  {
    case MODULE_NORMAL_OPERATION:
      if(!isDateTimeUpdated(packetTimeStamp))
      {
        if (loadDateTimeToBuffer(tmpBuffer))
          sendCmdBuff(ROUTER_COMMAND | COMMAND_SOURCE_ROUTER | CMD_SET_DATETIME, tmpBuffer, sizeof(tmpBuffer), DIRECT_TO_LORA);
      }
      else
        sendCmd(ROUTER_COMMAND | CMD_POWER_DOWN, DIRECT_TO_LORA);
      break;
    case MODULE_REQUEST_CONFIG_MODE:
      tmpBuffer[0] = 0;
      sendCmdBuff(ROUTER_COMMAND | CMD_SET_TIMEOUT, tmpBuffer, 1, DIRECT_TO_LORA);
      break;
    case MODULE_CONFIG_MODE_ON:
      break;
    default:
      ModuleOperationMode = MODULE_NORMAL_OPERATION;
      break;
  }
}

static void taskInternalCommandHandler(void *pvParameters)
{
  char txBuffer[MAX_PACKET_SIZE];
  uint8_t retransmissionCounter;
  
  for(;;)
  {
    if(xQueueReceive(internalCommandQueue, &txBuffer, (TickType_t)portMAX_DELAY) == pdPASS)
    {
      if(isWiFiConnected())
      {
        if((txBuffer[3] & COMMAND_MASK) == CMD_GET_DATETIME)
        {
          sendDateTime(getPacketOrigin(txBuffer[3]));
        }
              
        if(isFirebaseReady() && ((txBuffer[3] & COMMAND_MASK) == CMD_SEND_SAMPLES))
        {
          // Espera enviar para o servidor
          retransmissionCounter = 0;
          while(!sendDataToDatabase(txBuffer) && retransmissionCounter < MAX_RETRANSMISSIONS)
          {
            vTaskDelay( 10 / portTICK_PERIOD_MS );
            retransmissionCounter++;
          }
        }
      }

      if((txBuffer[3] & COMMAND_MASK) == CMD_SET_TIMEOUT)
      {
        processSetTimeout(getPacketOrigin(txBuffer[3]), txBuffer[4]);
      }

      if(((txBuffer[3] & COMMAND_MASK) == CMD_REQUEST_ACTION) && (getPacketOrigin(txBuffer[3]) == COMMAND_SOURCE_MODULE))
      {
        struct tm timeinfo;

        getDateTimeFromBuffer(&timeinfo, &txBuffer[4]);
        processModuleActionRequest(&timeinfo);
      }
    }
  }
}

void initMessageManager(void)
{
  memset(&serialComm, 0, sizeof(serialComm));
  memset(&wifiComm, 0, sizeof(wifiComm));
  memset(&loraComm, 0, sizeof(loraComm));

  serialComm.transmissionQueue = xQueueCreate(MESSAGE_QUEUE_SIZE, MAX_PACKET_SIZE);
  wifiComm.transmissionQueue = xQueueCreate(MESSAGE_QUEUE_SIZE, MAX_PACKET_SIZE);
  loraComm.transmissionQueue = xQueueCreate(MESSAGE_QUEUE_SIZE, MAX_PACKET_SIZE);
  selectQueue = xQueueCreate(MESSAGE_QUEUE_SIZE, MAX_PACKET_SIZE);
  serialComm.forwardQueue = loraComm.transmissionQueue;
  wifiComm.forwardQueue = loraComm.transmissionQueue;
  loraComm.forwardQueue = selectQueue;

  // Estes campos foram criados para definir a origem dos pacotes, assim nenhum pacote pode ter uma origem falseada
  serialComm.originMask = COMMAND_SOURCE_SOFTWARE;
  wifiComm.originMask = COMMAND_SOURCE_SOFTWARE;
  loraComm.originMask = COMMAND_SOURCE_MODULE;

  internalCommandQueue = xQueueCreate(MESSAGE_QUEUE_SIZE, MAX_PACKET_SIZE);

  initSerialManager(&serialComm);
  initWiFiManager(&wifiComm);
  initLoRaManager(&loraComm);

  xTaskCreate(taskSelectQueue, "taskSelectQueue", 8192, NULL, 2, NULL);
  xTaskCreate(taskInternalCommandHandler, "taskInternalCommandHandler", 8192, NULL, 2, NULL);
}

//==================================================================================================
