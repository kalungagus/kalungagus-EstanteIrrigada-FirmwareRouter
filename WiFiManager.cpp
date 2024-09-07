//==================================================================================================
// Includes
//==================================================================================================
#include <WiFi.h>
#include <NetBIOS.h>
#include <AsyncUDP.h>
#include <Firebase_ESP_Client.h>
#include "SystemDefinitions.h"
#include "MessageManager.h"

//==================================================================================================
// Variáveis do módulo
//==================================================================================================
xTaskHandle taskOnline;
xTaskHandle taskOnlineTransmission;
xTaskHandle taskOffline;
AsyncUDP udp;

int connectionIdleCounter = 0;
int disconnectedCounter = 0;
bool connectedToClient=false;

FirebaseJson json;
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
String uid, databasePath;

WiFiServer server(5000);
WiFiClient client;

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
  server.close();
  WiFi.disconnect();
  vTaskSuspend(taskOnline);
  vTaskSuspend(taskOnlineTransmission);
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

void setupFirebase(void)
{
  // Define a API Key para o banco de dados Firebase
  config.api_key = WEB_API_KEY;

  // Define as credenciais de usuário para o acesso ao banco de dados
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;

  // Atribui o link para a base de dados
  config.database_url = DATABASE_LINK;

  // Conecta à base de dados, ou reconecta caso a conexão anterior tenha sido perdida.
  Firebase.reconnectWiFi(true);
  fbdo.setResponseSize(4096);

  // Todo: adaptar a função para o sistemas de mensagens do módulo
  // Assign the callback function for the long running token generation task
  //config.token_status_callback = tokenStatusCallback; //see addons/TokenHelper.h
  
  // Atribuir o máximo de tentativas de geração de token
  config.max_token_generation_retry = 5;

  // Inicializando a biblioteca com os dados configurados.
  Firebase.begin(&config, &auth);

  sendMessageWithNewLine("Obtendo UID do Firebase.", DIRECT_TO_SERIAL);
  while ((auth.token.uid) == "") 
    vTaskDelay( 10 / portTICK_PERIOD_MS );

  uid = auth.token.uid.c_str();
  sendMessage("User UID: ", DIRECT_TO_SERIAL);
  sendMessageWithNewLine(uid, DIRECT_TO_SERIAL);

  databasePath = "/UsersData/" + uid + "/amostras";
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
        vTaskResume(taskOnlineTransmission);
        setupUDP();
        setupFirebase();
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

uint16_t bcdToInt(uint8_t data)
{
  return ((((data & 0xF0) >> 4) * 10) + (data & 0x0F));
}

float getVoltage(uint16_t value)
{
  return ((3.3f/1024) * value);
}

void sendDataToDatabase(char *packet)
{
  if(packet[3] == CMD_GET_SAMPLES)  // Só envia amostras para a base de dados
  {
    char printBuffer[20];
    String parentPath;

    sprintf(printBuffer, "%02d/%02d/%04d %02d:%02d:%02d", bcdToInt(packet[6]), bcdToInt(packet[7]), bcdToInt(packet[4]) + 2000,
                                                          bcdToInt(packet[8]), bcdToInt(packet[11]), bcdToInt(packet[10]));
    json.set("/instant", String(printBuffer));
    json.set("/sensor1", String(getVoltage(*((uint16_t *)&packet[12]))));
    json.set("/sensor2", String(getVoltage(*((uint16_t *)&packet[14]))));
    json.set("/sensor3", String(getVoltage(*((uint16_t *)&packet[16]))));
    json.set("/sensor4", String(getVoltage(*((uint16_t *)&packet[18]))));
    json.set("/sensor5", String(getVoltage(*((uint16_t *)&packet[20]))));
    json.set("/sensor6", String(getVoltage(*((uint16_t *)&packet[22]))));
    json.set("/valvula1", String((uint8_t)packet[24]));
    json.set("/valvula2", String((uint8_t)packet[25]));
    json.set("/valvula3", String((uint8_t)packet[26]));
    json.set("/valvula4", String((uint8_t)packet[27]));
    json.set("/valvula5", String((uint8_t)packet[28]));
    json.set("/valvula6", String((uint8_t)packet[29]));

    // Cria um timestamp para a base de dados
    sprintf(printBuffer, "%02d%02d%02d%02d%02d%02d",  bcdToInt(packet[4]), bcdToInt(packet[7]), bcdToInt(packet[6]),
                                                      bcdToInt(packet[8]), bcdToInt(packet[11]), bcdToInt(packet[10]));
    parentPath = databasePath + "/" + String(printBuffer);
    Firebase.RTDB.setJSON(&fbdo, parentPath.c_str(), &json);
  }
}

void transmissionScheduler(void *pvParameters)
{
  commInterface_t *manager = (commInterface_t *)pvParameters;
  char txPacket[MAX_PACKET_SIZE];

  for(;;)
  {
    if(xQueueReceive(manager->transmissionQueue, &txPacket, (TickType_t)portMAX_DELAY) == pdPASS)
    {
      if(isClientConnected())
      {
        int length = txPacket[2] + 3;
        
        client.write(txPacket, length);
        client.flush();
      }

      if(Firebase.ready())
        sendDataToDatabase(txPacket);
    }
  }
}

void taskWiFiServer(void *pvParameters)
{
  commInterface_t *manager = (commInterface_t *)pvParameters;

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
  xTaskCreate(taskWiFiServer, "taskWiFiServer", 8192, manager, 1, &taskOnline);
  xTaskCreate(transmissionScheduler, "TransmissionScheduler", 8192, manager, 1, &taskOnlineTransmission);
  vTaskSuspend(taskOnline);
  vTaskSuspend(taskOnlineTransmission);

  sendMessageWithNewLine("Gerenciador de WiFi configurado.", DIRECT_TO_SERIAL);
  sendMessageWithNewLine("Iniciar conexao...", DIRECT_TO_SERIAL);
}

//==================================================================================================
