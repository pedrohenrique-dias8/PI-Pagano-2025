// Inclusão das bibliotecas
#include <WiFi.h>
#include <PubSubClient.h>
#include <WebServer.h>
#include <Preferences.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// Pinos para comunicação com o sensor de CO2
#define RXD2 16
#define TXD2 17

String ips[] = {"192.168.1.101", "192.168.1.102", "192.168.1.103"};
String ids[] = {"ESP_B8670D", "ESP_AF3FE7", "ESP_B2492D"};

String estados[] = {"off", "off", "off"}; 
String estados_atuais[] = {"off", "off", "off"};
String estados_anteriores[] = {"off", "off", "off"};

Preferences preferences;  // Armazena credenciais Wi-Fi na memória flash

// Configurações MQTT
const char* mqtt_server = "192.168.0.137";     // IP do Broker Mosquitto
const char* command_topic = "COMANDO-LCA";  //Tópico para leitura do sinal de comando
const char* publish_topic = "CO2-LCA";      //Tópico para envio dos dados obtidos
WiFiClient espClient;
PubSubClient client(espClient);

String ssid;
String password;

WebServer server(80);

int CO2 = 0;
bool botao = false;  // Estado do botão via MQTT
bool manual = false;

// Definição dos pinos dos LEDs
#define LED_GREEN 21
#define LED_YELLOW 22
#define LED_RED 23
#define LED_MQTT 2  // LED controlado pelo MQTT

// Controle do tempo para substituir o delay
unsigned long previousLoopMillis = 0;
unsigned long previousSendMillis = 0;
unsigned long manualStartMillis = 0;

const long sendInterval = 10*1000;  // Intervalo de 10 segundos para enviar as mensagens
const long loopInterval = 2*1000;   // Intervalo de 2 segundos para ler o CO2
const long manualTimeout = 20*1000;  // Intervalo de 20 minutos para reativar o controle

int histerese = 20;
int leiturasInvalidas = 0;
// Função para capturar os dados do sensor de CO₂
int gas_concentration_uart() {
  byte addArray[] = { 0xFF, 0x01, 0x86, 0x00, 0x00, 0x00, 0x00, 0x00, 0x79 };  //A leitura do nível é feita nestes endereços
  char dataValue[9];

  Serial2.write(addArray, 9);
  Serial2.readBytes(dataValue, 9);

  int resHigh = (int)dataValue[2];
  int resLow = (int)dataValue[3];
  return (resHigh * 256) + resLow;
}

// Função para processar mensagens recebidas do MQTT - Acionamento
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Mensagem recebida no tópico: ");
  Serial.println(topic);

  String message;
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  Serial.print("Mensagem: ");
  Serial.println(message);

  // Verifica o tópico COMANDO-SALA1 e altera o LED de acordo com a mensagem
  if (String(topic) == command_topic) {
    if (message == "1") {
      digitalWrite(LED_MQTT, HIGH);  // Liga o LED MQTT
      botao = true;
    } else if (message == "0") {
      digitalWrite(LED_MQTT, LOW);  // Desliga o LED MQTT
      botao = false;
    }
  }
}

// Função para conectar ao MQTT
void reconnect() {
  while (!client.connected()) {
    Serial.print("Conectando ao broker MQTT... ");
    if (client.connect("ESP32-" + espClient)) {
      Serial.println("Conectado!");
      //client.subscribe(command_topic);  // Inscreve-se para receber comandos
    } else {
      Serial.print("Falha. Código: ");
      Serial.println(client.state());
      return ;
    }
  }
}
// Essa função permite a troca de rede wifi pela serial
// Quando ligar, tem 5s para reponder se quer ou não trocar a rede
// Se aceitar, é só enviar o nome da rede (SSID) e depois a senha
void changeWiFiCredentials() {
  Serial.println("\nTrocar rede padrão? (s/n) - Aguarde 5s para ignorar.");

  unsigned long startTime = millis();
  char resposta = '\0';

  // Espera por uma resposta ou timeout de 5 segundos
  while (millis() - startTime < 5000) {
    if (Serial.available()) {
      resposta = Serial.read();
      break;
    }
  }

  if (resposta != 's' && resposta != 'S') {
    Serial.println("Tempo expirado ou resposta inválida. Mantendo rede atual.");
    return;
  }

  // Limpa o buffer da serial para evitar leitura incorreta
  while (Serial.available()) {
    Serial.read();
  }

  Serial.println("Digite o novo SSID:");
  while (!Serial.available())
    ;
  String newSSID = Serial.readStringUntil('\n');
  newSSID.trim();


  Serial.println("Digite a nova senha:");
  while (!Serial.available())
    ;
  String newPassword = Serial.readStringUntil('\n');
  newPassword.trim();

  // Salvar novas credenciais na memória flash
  preferences.begin("wifi", false);
  preferences.putString("ssid", newSSID);
  preferences.putString("password", newPassword);
  preferences.end();

  Serial.println("Novas credenciais salvas! Reiniciando...");
  ESP.restart();
}

// Essa função faz a conexão com a rede wifi usando as informações guardadas na memória
void connectWiFi() {
  Serial.print("Conectando ao Wi-Fi: ");
  Serial.println(ssid);

  WiFi.begin(ssid.c_str(), password.c_str());

  unsigned long startAttemptTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 15000) {
    delay(500);
    Serial.print("..");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConectado!");
    Serial.print("IP do ESP32: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nFalha ao conectar! Verifique as credenciais.");
  }
}
// Função para verificação do estado de um relé
void consultarEstadosSonoff(String ips[], String ids[], String estados_atuais[]) {
  for (int i = 0; i < 3; i++) {
    HTTPClient http;
    http.setTimeout(1500);
    http.setConnectTimeout(1500);

    String url = "http://" + ips[i] + ":8081/zeroconf/info";
    http.begin(url);
    http.addHeader("Content-Type", "application/json");

    // Corpo JSON
    String corpo = "{\"deviceid\":\"" + ids[i] + "\",\"data\":{}}";

    int httpCode = http.POST(corpo);

    if (httpCode >= 200 && httpCode < 300) {
      String resposta = http.getString();

      if (resposta.indexOf("\"switch\":\"on\"") != -1) {
        estados_atuais[i] = "on";
      } else if (resposta.indexOf("\"switch\":\"off\"") != -1) {
        estados_atuais[i] = "off";
      } else {
        estados_atuais[i] = "";  // resposta inesperada
      }

      Serial.println("Relé " + ids[i] + " estado atual: " + estados_atuais[i]);

      // ✅ Verifica acionamento manual SOMENTE se houve leitura válida
      if (estados_atuais[i] != "" &&
          !manual &&
          estados_atuais[i] != estados_anteriores[i] &&
          estados[i] == estados_anteriores[i]) {
        manual = true;
        Serial.println("Acionamento manual detectado. Automação pausada por 20 minutos.");
      }
    } else {
      Serial.println("Erro ao consultar relé " + ids[i] + ": " + http.errorToString(httpCode));
      estados_atuais[i] = "";  // falha na consulta, ignora essa leitura
    }

    http.end();
  }
}
// Função para enviar comandos aos relés
void enviarComandoSonoff(String ips[], String ids[], String estados[], String estados_anteriores[]) {
  for (int i = 0; i < 3; i++) {
    HTTPClient http;
    String url = "http://" + ips[i] + ":8081/zeroconf/switch";
    http.setTimeout(1500);
    http.setConnectTimeout(1500);
    http.begin(url);
    http.addHeader("Content-Type", "application/json");

    // Monta o corpo JSON
    StaticJsonDocument<256> doc;
    doc["deviceid"] = ids[i];
    doc["data"]["switch"] = estados[i];
    
    String body;
    serializeJson(doc, body);

    int httpCode = http.POST(body);

    if (httpCode >= 200 && httpCode < 300) {
      Serial.println("Comando enviado com sucesso para " + ids[i] + ": " + estados[i]);
      estados_anteriores[i] = estados[i];  // Só atualiza se for bem-sucedido
    } else {
      Serial.println("Erro ao enviar comando para " + ids[i] + ": " + http.errorToString(httpCode));
      // estados_anteriores[i] permanece como estava
    }

    http.end();
  }
}


bool verificarLeituraCO2(int ppm) {
  if (ppm < 250 || ppm > 5000) {
    leiturasInvalidas++;

    Serial.print("Leitura inválida de CO2: ");
    Serial.println(ppm);

    if (leiturasInvalidas >= 5) {
      Serial.println("Muitas leituras inválidas. Reiniciando ESP...");
      delay(2000);
      ESP.restart();
    }

    return false;  // Leitura inválida
  }

  // Se chegou aqui, a leitura foi válida
  leiturasInvalidas = 0;
  return true;
}

String montarPaginaHTML() {
  String html = "<!DOCTYPE html><html lang='pt-BR'><head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<meta http-equiv='refresh' content='5'>";
  html += "<title>Monitor de CO₂</title>";
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; background-color: #f0f0f0; text-align: center; padding: 20px; }";
  html += "h1 { color: #333; }";
  html += ".co2 { font-size: 2em; margin: 20px 0; }";
  html += ".status { margin: 10px auto; max-width: 400px; }";
  html += ".relay { background: white; margin: 10px 0; padding: 10px; border-radius: 10px; box-shadow: 0 0 5px rgba(0,0,0,0.1); }";
  html += ".on { color: green; font-weight: bold; }";
  html += ".off { color: red; font-weight: bold; }";
  html += ".manual { color: orange; font-weight: bold; }";
  html += "</style></head><body>";

  html += "<h1>Monitor de CO₂</h1>";
  html += "<div class='co2'>🟢 <strong>" + String(CO2) + " ppm</strong></div>";
  html += "<p><small>Atualiza a cada 5 segundos</small></p>";

  html += "<div class='status'>";
  for (int i = 0; i < 3; i++) {
    html += "<div class='relay'>";
    html += "Relé <strong>" + ids[i] + "</strong>: ";
    if (estados_atuais[i] == "on") {
      html += "<span class='on'>Ligado</span>";
    } else if (estados_atuais[i] == "off") {
      html += "<span class='off'>Desligado</span>";
    } else {
      html += "<span style='color: gray;'>Desconhecido</span>";
    }
    html += "</div>";
  }
  html += "</div>";

  if (manual) {
    html += "<p class='manual'>⚠ Controle manual detectado. Automação pausada por 20 minutos.</p>";
  } else {
    html += "<p>Controle automático ativo.</p>";
  }

  html += "</body></html>";
  return html;
}


void rotateLeft(String arr[], int size) {
  String temp = arr[0];
  for (int i = 0; i < size - 1; i++) {
    arr[i] = arr[i + 1];
  }
  arr[size - 1] = temp;
}

void executarComandoSensorCO2() {
  if (Serial.available()) {
    String comando = Serial.readStringUntil('\n');
    comando.trim();

    byte comandoUART[9];

    if (comando.equalsIgnoreCase("ativar")) {
      byte temp[] = { 0xFF, 0x01, 0x79, 0xA0, 0x00, 0x00, 0x00, 0x00, 0xE6 };
      memcpy(comandoUART, temp, 9);
      Serial.println("Auto calibração ATIVADA.");
    } 
    else if (comando.equalsIgnoreCase("desativar")) {
      byte temp[] = { 0xFF, 0x01, 0x79, 0x00, 0x00, 0x00, 0x00, 0x00, 0x86 };
      memcpy(comandoUART, temp, 9);
      Serial.println("Auto calibração DESATIVADA.");
    } 
    else if (comando.equalsIgnoreCase("calibrar")) {
      byte temp[] = { 0xFF, 0x01, 0x87, 0x00, 0x00, 0x00, 0x00, 0x00, 0x78 };
      memcpy(comandoUART, temp, 9);
      Serial.println("Calibração manual enviada (assumindo 400 ppm).");
    } 
    else {
      Serial.println("Comando inválido. Use: ativar, desativar ou calibrar");
      return;
    }

    Serial2.write(comandoUART, 9);
  }
}


void setup() {
  //Serial para comandos e visualização
  Serial.begin(115200);


  // Aguarda conexão com o terminal Serial por até 3 segundos
  unsigned long startTime = millis();
  bool serialConnected = false;
  while (millis() - startTime < 3000) {
    if (Serial) {
      serialConnected = true;
      break;
    }
  }

  //Serial para leitura de CO2
  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);

  // Define todos os leds como saida
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_YELLOW, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_MQTT, OUTPUT);

  // Acende todos os LEDs no início
  digitalWrite(LED_GREEN, HIGH);
  digitalWrite(LED_YELLOW, HIGH);
  digitalWrite(LED_RED, HIGH);
  digitalWrite(LED_MQTT, HIGH);
  // Aguarda 1 segundo
  delay(1000);
  // Apaga todos os LEDs
  digitalWrite(LED_GREEN, LOW);
  digitalWrite(LED_YELLOW, LOW);
  digitalWrite(LED_RED, LOW);
  digitalWrite(LED_MQTT, LOW);

  // Recuperar credenciais Wi-Fi salvas
  preferences.begin("wifi", true);
  ssid = preferences.getString("ssid", "defaultSSID");
  password = preferences.getString("password", "defaultPassword");
  preferences.end();

  // Pergunta se deseja trocar a rede
  if (serialConnected) {
    changeWiFiCredentials();
  }

  // Conectar ao Wi-Fi
  connectWiFi();

  // Configurar MQTT
  //client.setServer(mqtt_server, 1883);
  //client.setCallback(callback);
  // Conectar ao broker MQTT
  //reconnect();
  delay(500);
  enviarComandoSonoff(ips, ids, estados,estados_anteriores);
  server.on("/", []() {
    server.send(200, "text/html", montarPaginaHTML());
  });
  server.begin();
  Serial.println("Servidor web iniciado!");

  delay(5000);  //aguarda mais 5 segundos para iniciar o processo
}

void loop() {
  unsigned long currentMillis = millis();
  if (!client.connected()) {
      reconnect();
  }
  client.loop(); // Mantém a conexão MQTT ativa
  executarComandoSensorCO2();
  // Loop para conexão e leitura
  if (currentMillis - previousLoopMillis >= loopInterval) {
    previousLoopMillis = currentMillis;
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi desconectado. Tentando reconectar...");
      connectWiFi();
      return;
    }
    // Captura a concentração de CO₂
    CO2 = gas_concentration_uart();
    // Esta verificação tenta ler o valor de CO2  vezes
    // Se os valores forem absurdos, ele reinicia
    // Caso contrário, reseta o contador e continua normal

    if (!verificarLeituraCO2(CO2)) {
      return;  // ignora o resto do loop
    }

    Serial.print("CO2: ");  // Envia o valor de CO2 para a Serial
    Serial.println(CO2);
    server.handleClient();
  }

  // Loop de comando e envio de dados
  if (currentMillis - previousSendMillis >= sendInterval) {
    previousSendMillis = currentMillis;
    client.publish(publish_topic, String(CO2).c_str());
    if (!manual){
      manualStartMillis = millis();
        // Controle dos LEDs conforme os níveis de CO₂
      if ((CO2 < 650) && (manual==false)){
        digitalWrite(LED_GREEN, LOW);
        digitalWrite(LED_YELLOW, LOW);
        digitalWrite(LED_RED, LOW);

        rotateLeft(ips, 3);
        rotateLeft(ids, 3);
        // Desliga todos os relés
        estados[0] = "off";
        estados[1] = "off";
        estados[2] = "off";

      } else if ((CO2 >= 650 + histerese) && (CO2 < 900) && (manual==false)) {
        digitalWrite(LED_GREEN, HIGH);
        digitalWrite(LED_YELLOW, LOW);
        digitalWrite(LED_RED, LOW);
        // Liga 1 relé
        estados[0] = "on";
        estados[1] = "off";
        estados[2] = "off";

      } else if ((CO2 >= 900 + histerese) && (CO2 < 1200) && (manual==false)) {
        digitalWrite(LED_GREEN, LOW);
        digitalWrite(LED_YELLOW, HIGH);
        digitalWrite(LED_RED, LOW);
        // Liga 2 relés
        estados[0] = "on";
        estados[1] = "on";
        estados[2] = "off";

      } else if ((CO2 > 1200 + histerese) && (manual==false)) {
        digitalWrite(LED_GREEN, LOW);
        digitalWrite(LED_YELLOW, LOW);
        digitalWrite(LED_RED, HIGH);
        // Liga os 3 relés
        estados[0] = "on";
        estados[1] = "on";
        estados[2] = "on";
      }
    consultarEstadosSonoff(ips, ids, estados_atuais);
    if (!manual){
      enviarComandoSonoff(ips, ids, estados, estados_anteriores);
    }
    
    }
    else if (manual && millis() - manualStartMillis >= manualTimeout) {
      manual = false;
      Serial.println("Tempo de controle manual expirado. Retornando ao modo automático.");
      enviarComandoSonoff(ips, ids, estados, estados_anteriores);
    }
  }
}
