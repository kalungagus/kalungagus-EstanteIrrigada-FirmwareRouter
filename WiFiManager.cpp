//==================================================================================================
// Includes
//==================================================================================================
#include <WiFi.h>
#include <NetBIOS.h>
#include <AsyncUDP.h>
#include "SystemDefinitions.h"
#include "SerialManager.h"
#include "System.h"

//==================================================================================================
// Macros
//==================================================================================================
#define DEBUG()           digitalWrite(DEBUG_PIN, !digitalRead(DEBUG_PIN))

//==================================================================================================
// Funções do módulo, declaradas aqui para melhor organização do arquivo
//==================================================================================================
void initWiFiManager(uint8_t wifiState);
void connectToWiFi(void);
void taskCheckWiFiStatus(void *);
bool isWiFiConnected(void);
void resetWiFiConnection(void);
void setupWiFi(void);
void transmissionScheduler(void *);
void listNetworks(void);
void printEncryptionType(int thisType);
void udpProcessing(void *pvParameters);

//==================================================================================================
// Variáveis do módulo
//==================================================================================================
xTaskHandle taskOnline;
xTaskHandle taskOffline;
xTaskHandle taskUdp;

extern QueueHandle_t messageQueue;

int connectionIdleCounter = 0;
int disconnectedCounter = 0;
bool connectedToClient=false;

AsyncUDP udp;
serialBuffer wifiReceivedData;

//==================================================================================================
// Funções
//==================================================================================================
void initWiFiManager(uint8_t wifiState)
{
  sendMessageWithNewLine("Configurando gerenciador de WiFi.", DIRECT_TO_SERIAL);
  memset(&wifiReceivedData, 0, sizeof(wifiReceivedData));

  xTaskCreate(taskCheckWiFiStatus, "CheckWiFiStatus", 8192, NULL, 1, &taskOffline);
  if(wifiState == 0)
    vTaskSuspend(taskOffline);
  xTaskCreate(transmissionScheduler, "TransmissionScheduler", 8192, NULL, 1, &taskOnline);
  vTaskSuspend(taskOnline);
  xTaskCreate(udpProcessing, "udpProcessing", 2000, NULL, 1, &taskUdp);
  vTaskSuspend(taskUdp);

  sendMessageWithNewLine("Gerenciador de WiFi configurado.", DIRECT_TO_SERIAL);
  if(wifiState == 0)
    sendMessageWithNewLine("Clique em ativar WiFi para iniciar conexao.", DIRECT_TO_SERIAL);
  else
  {
    sendMessageWithNewLine("Iniciar conexao...", DIRECT_TO_SERIAL);
    setupWiFi();
  }
}

bool isWiFiConnected(void)
{
  if(WiFi.status() == WL_CONNECTED)
    return(true);
  else
    return(false);
}

bool isClientConnected(void)
{
  return(connectedToClient);
}

void resetWiFiConnection(void)
{
  WiFi.disconnect();
  vTaskSuspend(taskOnline);
  vTaskResume(taskOffline);
}

void taskCheckWiFiStatus(void *pvParameters)
{
  char s[16];
  int updatedWiFiStatus;
  wl_status_t wifiStatus;

  for(;;)
  {
    vTaskDelay( 5000 / portTICK_PERIOD_MS );
    wifiStatus = WiFi.status();
    switch(wifiStatus)
    {
      case WL_CONNECTED:
        sendMessageWithNewLine("WiFi conectado.", DIRECT_TO_SERIAL);
        sendMessage("Endereco IP: ", DIRECT_TO_SERIAL);
        sendMessageWithNewLine(WiFi.localIP().toString(), DIRECT_TO_SERIAL);
        sendMessage("Hostname: ", DIRECT_TO_SERIAL);
        sendMessageWithNewLine(WiFi.getHostname(), DIRECT_TO_SERIAL);
        vTaskResume(taskOnline);
        vTaskResume(taskUdp);
        vTaskSuspend(taskOffline);
        break;
      case WL_NO_SHIELD:
        sendMessageWithNewLine("WL_NO_SHIELD retornado.", DIRECT_TO_SERIAL);
        connectToWiFi();
        break;
      case WL_IDLE_STATUS:
        sendMessageWithNewLine("WL_IDLE_STATUS retornado.", DIRECT_TO_SERIAL);
        connectionIdleCounter++;
        if(connectionIdleCounter >= MAX_CONNECTION_IDLE_STATUS)
        {
          connectionIdleCounter = 0;
          connectToWiFi();
        }
        break;
      case WL_NO_SSID_AVAIL:
        sendMessageWithNewLine("WL_NO_SSID_AVAIL retornado.", DIRECT_TO_SERIAL);
        connectToWiFi();
        break;
      case WL_SCAN_COMPLETED:
        sendMessageWithNewLine("WL_SCAN_COMPLETED retornado.", DIRECT_TO_SERIAL);
        break;
      case WL_CONNECT_FAILED:
        sendMessageWithNewLine("WL_CONNECT_FAILED retornado.", DIRECT_TO_SERIAL);
        break;
      case WL_CONNECTION_LOST:
        sendMessageWithNewLine("WL_CONNECTION_LOST retornado.", DIRECT_TO_SERIAL);
        connectToWiFi();
        break;
      case WL_DISCONNECTED:
        disconnectedCounter++;
        if(disconnectedCounter >= MAX_DISCONNECTED_MESSAGES)
        {
          sendMessageWithNewLine("Sistema nao consegue se conectar. Reiniciando para teste.", DIRECT_TO_SERIAL);
          delay(1000);
          ESP.restart();
        }
        else
        {
          setupWiFi();
          connectToWiFi();          
        }
        break;
      case WL_STOPPED:
        sendMessageWithNewLine("WL_STOPPED retornado.", DIRECT_TO_SERIAL);
        connectToWiFi();
        break;
      default:
        snprintf(s, 16, "%d", wifiStatus);
        sendMessage("WiFi.status() retornou ", DIRECT_TO_SERIAL);
        sendMessage(s, DIRECT_TO_SERIAL);
        sendMessageWithNewLine(" para o modulo.", DIRECT_TO_SERIAL);
        break;
    }
  }
}

// Detalhes em https://github.com/esp8266/Arduino/issues/4352
void setupWiFi(void)
{
  WiFi.mode(WIFI_STA);
  WiFi.persistent(false);
  WiFi.disconnect(true);
  delay(100);
  WiFi.setAutoReconnect(false);
  if( WiFi.getAutoReconnect() ) WiFi.setAutoReconnect( false );
}

void connectToWiFi(void)
{
  char ssid[SSID_LENGHT] = DEFAULT_SSID;
  char password[PASSWORD_LENGTH] = DEFAULT_PASSWORD;
  String hostname = DEFAULT_HOSTNAME;

  sendMessageWithNewLine("Iniciando conexao com Wifi", DIRECT_TO_SERIAL);
  WiFi.setHostname(hostname.c_str());
  WiFi.begin(ssid, password);
  delay(500);
  NBNS.begin(hostname.c_str());
}

void setWifiOn(void)
{
  sendMessageWithNewLine("Ligando Wifi.", DIRECT_TO_SERIAL);
  vTaskResume(taskOffline);
  setupWiFi();
}

void setWiFiOff(void)
{
  sendMessageWithNewLine("Desligando Wifi.", DIRECT_TO_SERIAL);

  if(isWiFiConnected())
  {
    WiFi.disconnect();
    vTaskSuspend(taskOnline);
  }

  vTaskSuspend(taskOffline);
  WiFi.mode(WIFI_OFF);
  sendMessageWithNewLine("Wifi desligada.", DIRECT_TO_SERIAL);
}

void listNetworks(void) 
{
  char s[16];
  sendMessageWithNewLine("** Scan Networks **", DIRECT_TO_SERIAL);
  int numSsid = WiFi.scanNetworks();
  if (numSsid == -1) 
  {
    sendMessageWithNewLine("Nao encontrou conexao de rede.", DIRECT_TO_SERIAL);
  }
  else
  {
    sendMessage("Redes disponiveis:", DIRECT_TO_SERIAL);
    snprintf(s, 16, "%d", numSsid);
    sendMessageWithNewLine(s, DIRECT_TO_SERIAL);
  
    for (int thisNet = 0; thisNet < numSsid; thisNet++) 
    {
      sendMessage(String(thisNet), DIRECT_TO_SERIAL);
      sendMessage(") ", DIRECT_TO_SERIAL);
      sendMessage(WiFi.SSID(thisNet), DIRECT_TO_SERIAL);
      sendMessage("\tSignal: ", DIRECT_TO_SERIAL);
      snprintf(s, 16, "%d", WiFi.RSSI(thisNet));
      sendMessage(s, DIRECT_TO_SERIAL);
      sendMessage(" dBm", DIRECT_TO_SERIAL);
      sendMessage("\tEncryption: ", DIRECT_TO_SERIAL);
      snprintf(s, 16, "%d", WiFi.encryptionType(thisNet));
      sendMessageWithNewLine(s, DIRECT_TO_SERIAL);
      vTaskDelay( 50 / portTICK_PERIOD_MS );
    }
  }
}

void transmissionScheduler(void *pvParameters)
{
  WiFiServer server(5000);
  WiFiClient client;
  char txPacket[MAX_PACKET_SIZE];

  sendMessageWithNewLine("Task servidor iniciada.", DIRECT_TO_SERIAL);
  server.begin();
  for(;;)
  {
    client = server.available();

    if(client)
    {
      sendMessageWithNewLine("Conexao com cliente estabelecida.", DIRECT_TO_SERIAL);
      connectedToClient = true;
      while (client.connected()) 
      {
        if(xQueueReceive(messageQueue, &txPacket, (TickType_t)0) == pdPASS)
        {
          int length = txPacket[2] + 3;
          
          client.write(txPacket, length);
          client.flush();
        }
        
        if(client.available())
        {
          char receivedData = client.read();
          processCharReception(receivedData, &wifiReceivedData);
        }
        
        vTaskDelay( 10 / portTICK_PERIOD_MS );
      }
      sendMessageWithNewLine("Conexao com cliente encerrada.", DIRECT_TO_SERIAL);
      client.stop();
      connectedToClient = false;
    }
    else
    {
      vTaskDelay( 10 / portTICK_PERIOD_MS );
    }
  }
}

void udpProcessing(void *pvParameters)
{
  sendMessageWithNewLine("Task udp iniciada.", DIRECT_TO_SERIAL);

  for(;;)
  {
    if(udp.listen(30000)) 
    {
      udp.onPacket([](AsyncUDPPacket packet) {
        if(packet.isBroadcast())
        {
          if(packet.length() == 3 && packet.data()[0] == 'S' && packet.data()[1] == 'C' && packet.data()[2] == 'H')
          {
            IPAddress myIP = WiFi.localIP();
            unsigned char decodedIP[7] = {'S', 'C', 'H', myIP[0], myIP[1], myIP[2], myIP[3]};
            
            sendMessageWithNewLine("Broadcast recebido.", DIRECT_TO_SERIAL);
            udp.broadcastTo(decodedIP, 7, 30000);
          }
        }
      });
    }
    else
      vTaskDelay( 100 / portTICK_PERIOD_MS );
  }
}

//==================================================================================================
