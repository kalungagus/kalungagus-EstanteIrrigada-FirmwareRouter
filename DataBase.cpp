//==================================================================================================
// Includes
//==================================================================================================
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>
#include "SystemDefinitions.h"
#include "MessageManager.h"

//==================================================================================================
// Variáveis do módulo
//==================================================================================================
xTaskHandle taskDataBase;
QueueHandle_t dataBaseMessages;

FirebaseJson json;
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
String uid, databasePath;
bool firebaseServerReady = false;
bool sendDataToServerEnabled = true;

//==================================================================================================
// Funções
//==================================================================================================
// Substitui a função padrão para utilizar as funções de impressão definidas pelo projeto.
void myTokenStatusCallback(TokenInfo info)
{
  if (info.status == token_status_error)
  {
    sendMessageWithNewLine("Token info: type = " + String(getTokenType(info)), DIRECT_TO_SERIAL);
    sendMessageWithNewLine("Token info: status = " + String(getTokenStatus(info)), DIRECT_TO_SERIAL);
    sendMessageWithNewLine("Token error: " + getTokenError(info), DIRECT_TO_SERIAL);
  }
  else
  {
    sendMessageWithNewLine("Token info: type = " + String(getTokenType(info)), DIRECT_TO_SERIAL);
    sendMessageWithNewLine("Token info: status = " + String(getTokenStatus(info)), DIRECT_TO_SERIAL);
  }
}

void sendDataToDatabase(char *packet)
{
  xQueueSend(dataBaseMessages, (void *)packet, (TickType_t)0);
}

void setSendDataToServerEnabled(bool value)
{
  sendDataToServerEnabled = value;
}

float getVoltage(uint16_t value)
{
  return ((3.3f/1024) * value);
}

bool formatAndUploadData(char *packet)
{
  char printBuffer[30];
  String parentPath;
  bool response;

  sprintf(printBuffer, "%04d-%02d-%02dT%02d:%02d:%02d-03:00", bcdToInt(packet[4]) + 2000, bcdToInt(packet[7]), bcdToInt(packet[6]),
                                                              bcdToInt(packet[8]), bcdToInt(packet[11]), bcdToInt(packet[10]));

  if(sendDataToServerEnabled)
  {
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

    // As tarefas executadas aqui tomam um bom tempo. Se o servidor demorar também, o watchdog é ativado.
    // Assim, estou colocando este vTaskDelay para que o sistema possa executar algumas tarefas,
    // incluindo o tratamento de watchdog.
    vTaskDelay( 1 / portTICK_PERIOD_MS );
    
    response = Firebase.RTDB.setJSON(&fbdo, parentPath.c_str(), &json);
    if(!response)
      sendMessageWithNewLine("Erro no envio: " + fbdo.errorReason(), PRIORITY_SELECT);
  }
  else
  {
    sendMessageWithNewLine(String(printBuffer), PRIORITY_SELECT);
    response = true;
  }

  return response;
}

void setupDataBase(void)
{
  // Explicação do código de exemplo:
  // Since Firebase v4.4.x, BearSSL engine was used, the SSL buffer need to be set.
  // Large data transmission may require larger RX buffer, otherwise connection issue or data read time out can be occurred.
  fbdo.setBSSLBufferSize(2048 /* Rx buffer size in bytes from 512 - 16384 */, 1024 /* Tx buffer size in bytes from 512 - 16384 */);

  // Conecta à base de dados, ou reconecta caso a conexão anterior tenha sido perdida.
  Firebase.reconnectWiFi(false);
  fbdo.setResponseSize(4096);

  // Define um timeout de resposta para o Banco de dados.
  Firebase.RTDB.setReadTimeout(&fbdo, 10000);
  Firebase.config.timeout.serverResponse = 5000; // em milissegundos

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

  vTaskResume(taskDataBase);
}

void disconnectDataBase(void)
{
  vTaskSuspend(taskDataBase);
}

void taskBDStatus(void *pvParameters)
{
  char txPacket[MAX_PACKET_SIZE];

  for(;;)
  {
    // A documentação do módulo de Firebase diz para chamar Firebase.ready() repetidamente
    // para processamento de tarefas de autenticação
    if(Firebase.ready())
    {
      if(xQueueReceive(dataBaseMessages, &txPacket, (TickType_t) 0) == pdPASS)
      {
        if(!formatAndUploadData(txPacket))
        {
          // Item não pôde ser enviado. Recoloca na fila.
          xQueueSendToFront(dataBaseMessages, (void *)txPacket, (TickType_t)0);
        }
      }
    }
    vTaskDelay( 10 / portTICK_PERIOD_MS );
  }
}

void initDataBaseManager(void)
{
  sendMessageWithNewLine("Configurando gerenciador de Banco de dados.", DIRECT_TO_SERIAL);

  // Define a API Key para o banco de dados Firebase
  config.api_key = WEB_API_KEY;

  // Define as credenciais de usuário para o acesso ao banco de dados
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;

  // Atribui o link para a base de dados
  config.database_url = DATABASE_LINK;

  // Define a função de callback 
  config.token_status_callback = myTokenStatusCallback;

  dataBaseMessages = xQueueCreate(MESSAGE_QUEUE_SIZE, MAX_PACKET_SIZE);

  // Define a task de gerenciamento de conexão com banco de dados
  xTaskCreatePinnedToCore(taskBDStatus, "BDStatus", 8192, NULL, 1, &taskDataBase, 1);
  vTaskSuspend(taskDataBase);

  sendMessageWithNewLine("Gerenciador de Banco de dados configurado.", DIRECT_TO_SERIAL);
}