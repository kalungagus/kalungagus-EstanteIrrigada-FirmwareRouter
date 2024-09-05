//==================================================================================================
// Includes
//==================================================================================================
#include <WiFi.h>
#include <NetBIOS.h>
#include <AsyncUDP.h>
#include "SystemDefinitions.h"
#include "MessageManager.h"

//==================================================================================================
// Variáveis do módulo
//==================================================================================================
xTaskHandle taskOnline;
xTaskHandle taskOffline;
AsyncUDP udp;

int connectionIdleCounter = 0;
int disconnectedCounter = 0;
bool connectedToClient=false;

//==================================================================================================
// Funções
//==================================================================================================
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

void setupUDP(void)
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
}

void taskCheckWiFiStatus(void *pvParameters)
{
  for(;;)
  {
    vTaskDelay( 5000 / portTICK_PERIOD_MS );
    
    switch(WiFi.status())
    {
      case WL_CONNECTED:
        sendMessageWithNewLine("WiFi conectado.", DIRECT_TO_SERIAL);
        sendMessage("Endereco IP: ", DIRECT_TO_SERIAL);
        sendMessageWithNewLine(WiFi.localIP().toString(), DIRECT_TO_SERIAL);
        sendMessage("Hostname: ", DIRECT_TO_SERIAL);
        sendMessageWithNewLine(WiFi.getHostname(), DIRECT_TO_SERIAL);
        vTaskResume(taskOnline);
        setupUDP();
        vTaskSuspend(NULL);   // A task se suspende
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
        break;
    }
  }
}

void transmissionScheduler(void *pvParameters)
{
  commInterface_t *manager = (commInterface_t *)pvParameters;
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
        if(xQueueReceive(manager->transmissionQueue, &txPacket, (TickType_t)0) == pdPASS)
        {
          int length = txPacket[2] + 3;
          
          client.write(txPacket, length);
          client.flush();
        }
        
        if(client.available())
        {
          char receivedData = client.read();
          processCharReception(receivedData, manager);
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

void initWiFiManager(commInterface_t *manager)
{
  sendMessageWithNewLine("Configurando gerenciador de WiFi.", DIRECT_TO_SERIAL);

  setupWiFi();

  xTaskCreate(taskCheckWiFiStatus, "CheckWiFiStatus", 8192, NULL, 1, &taskOffline);
  xTaskCreate(transmissionScheduler, "TransmissionScheduler", 8192, manager, 1, &taskOnline);
  vTaskSuspend(taskOnline);

  sendMessageWithNewLine("Gerenciador de WiFi configurado.", DIRECT_TO_SERIAL);
  sendMessageWithNewLine("Iniciar conexao...", DIRECT_TO_SERIAL);
}

//==================================================================================================
