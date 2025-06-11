// Inclusão das bibliotecas
#include <WiFi.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>
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
String leitura_estados[] = {"off", "off", "off"};

Preferences preferences;  // Armazena credenciais Wi-Fi na memória flash

// Configurações MQTT
const char* mqtt_server = "192.168.0.137";     // IP do Broker Mosquitto
//const char* command_topic = "COMANDO-POSAUTO";  //Tópico para leitura do sinal de comando
String publish_topic = "CO2-POSAUTO1";      //Tópico para envio dos dados obtidos
WiFiClientSecure espClient;
PubSubClient client(espClient); //mudou aqui


String ssid;
String password;

WebServer server(80);

int CO2 = 0;
bool botao = false;  // Estado do botão via MQTT

// Definição dos pinos dos LEDs
#define LED_GREEN 21
#define LED_YELLOW 22
#define LED_RED 23
#define LED_MQTT 2  // LED controlado pelo MQTT

// Controle do tempo para substituir o delay
unsigned long previousLoopMillis = 0;
unsigned long previousSendMillis = 0;

const long sendInterval = 10000;  // Intervalo de 5 segundos para enviar as mensagens
const long loopInterval = 2000;

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
void reconnect() { //mudou toda funcao
  while (!client.connected()) {
    Serial.print("Conectando ao broker MQTT seguro... ");
    String clientId = "ESP32Client-" + String(random(0xffff), HEX);
    if (client.connect(clientId.c_str(), "teste123", "Teste123")) {
      Serial.println("Conectado com sucesso!");
      client.subscribe(command_topic);
    } else {
      Serial.print("Falha. Código: ");
      Serial.print(client.state());
      Serial.println(" Tentando novamente em 5 segundos...");
      delay(5000);
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
    Serial.print("...");
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
String consultarEstadoSonoff(String ipRele, String idRele) {
  HTTPClient http;
  http.setTimeout(1000);  // Timeout rápido
  http.setConnectTimeout(1000);

  String url = "http://" + ipRele + ":8081/zeroconf/info";
  http.begin(url);
  http.addHeader("Content-Type", "application/json");

  // Monta o corpo JSON
  String corpo = "{\"deviceid\":\"" + idRele + "\",\"data\":{}}";

  String estado = "";  // Inicializa como vazio, sem alteração até que a requisição seja bem-sucedida

  int httpCode = http.POST(corpo);

  // Verifica se o código HTTP está na faixa 2xx (sucesso)
  if (httpCode >= 200 && httpCode < 300) {
    String resposta = http.getString();

    // Verifica se a resposta contém "switch":"on" ou "switch":"off"
    if (resposta.indexOf("\"switch\":\"on\"") != -1) {
      estado = "on";  // Relé está ligado
    } else if (resposta.indexOf("\"switch\":\"off\"") != -1) {
      estado = "off";  // Relé está desligado
    }
  }

  http.end();
  return estado;  // Retorna o estado atualizado ou vazio se não houve alteração
}
// Função para enviar comandos aos relés
void enviarComandoSonoff(String ips[], String ids[], String estados[]) {
  for (int i = 0; i < 3; i++) {
    HTTPClient http;
    String url = "http://" + ips[i] + ":8081/zeroconf/switch";
    http.setTimeout(1000);
    http.setConnectTimeout(1000);
    http.begin(url);
    http.addHeader("Content-Type", "application/json");

    // Cria o JSON com ArduinoJson
    StaticJsonDocument<256> doc;
    doc["deviceid"] = ids[i];
    doc["data"]["switch"] = estados[i];

    // Serializa para uma string
    String body;
    serializeJson(doc, body);

    int httpCode = http.POST(body);

    if (httpCode > 0) {
      Serial.println("Comando enviado com sucesso para " + ids[i] + ": " + estados[i]);
    } else {
      Serial.println("Erro ao enviar comando para " + ids[i] + ": " + http.errorToString(httpCode));
    }

    http.end();
  }
}

bool leituraCO2Valida(int ppm) {
  return (ppm >= 250 && ppm <= 5000);  // Limites realistas do sensor
}

void publicarDadosMQTT(String estados[]) {
  // Publica valor de CO2
  client.publish((publish_topic + "/CO2").c_str(), String(CO2).c_str());

  // Publica estados dos exaustores
  client.publish((publish_topic + "/Exaustor1").c_str(), estados[0] == "on" ? "1" : "0");
  client.publish((publish_topic + "/Exaustor2").c_str(), estados[1] == "on" ? "1" : "0");
  client.publish((publish_topic + "/Exaustor3").c_str(), estados[2] == "on" ? "1" : "0");
}

String montarPaginaHTML() {
  String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'>";
  html += "<meta http-equiv='refresh' content='5'>";
  html += "<title>CO₂ Monitor</title></head><body>";
  html += "<h1>Nível de CO₂</h1>";
  html += "<p style='font-size:24px;'>🟢 <strong>" + String(CO2) + " ppm</strong></p>";
  html += "<p><small>Atualiza a cada 5 segundos</small></p>";
  html += "</body></html>";
  return html;
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
  client.setServer("c23e05b057fc4830861c79e7d6aefd0b.s1.eu.hivemq.cloud", 8883); //mudou aqui
  espClient.setInsecure();  // permite conexão TLS sem validar certificado

  client.setCallback(callback);
  // Conectar ao broker MQTT
  reconnect();
  delay(500);
  //enviarComandoSonoff(ips, ids, estados);
  // server.on("/", []() {
  //   server.send(200, "text/html", montarPaginaHTML());
  // });
  // server.begin();
  //Serial.println("Servidor web iniciado!");

  delay(5000);  //aguarda mais 5 segundos para iniciar o processo
}

bool manual = false;

void loop() {
  unsigned long currentMillis = millis();
  if (!client.connected()) {
      reconnect();
  }
  client.loop(); // Mantém a conexão MQTT ativa

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
    if (!leituraCO2Valida(CO2)) {
      leiturasInvalidas++;

      Serial.print("Leitura inválida de CO2: ");
      Serial.println(CO2);

      if (leiturasInvalidas >= 5) {
        Serial.println("Muitas leituras inválidas. Reiniciando ESP...");
        delay(2000);
        ESP.restart();  // reinicia o ESP32
      }

      return;  // ignora o resto do loop
    } else {
      leiturasInvalidas = 0;  // zera se uma leitura for boa
    }
    Serial.print("CO2: ");  // Envia o valor de CO2 para a Serial
    Serial.println(CO2);
    Serial.print("exaustores: ");  // Envia o valor de CO2 para a Serial
    Serial.println(estados[0] + estados[1] + estados[2]);
    //server.handleClient();
  }

  // Controle Serial para ligar/desligar os relés manualmente
  /*
  if (Serial.available()) {
      String comando = Serial.readStringUntil('\n');
      comando.trim();

      if (comando.equalsIgnoreCase("ligar")) {
          enviarComandoSonoff(IpRele1, idRele1, "on");
          enviarComandoSonoff(IpRele2, idRele2, "on");
          enviarComandoSonoff(IpRele3, idRele3, "on");
          Serial.println("Relés ligados manualmente via Serial.");
      } else if (comando.equalsIgnoreCase("desligar")) {
          enviarComandoSonoff(IpRele1, idRele1, "off");
          enviarComandoSonoff(IpRele2, idRele2, "off");
          enviarComandoSonoff(IpRele3, idRele3, "off");
          Serial.println("Relés desligados manualmente via Serial.");
      }
  }
  */

  // Loop de comando e envio de dados
  if (currentMillis - previousSendMillis >= sendInterval) {
    previousSendMillis = currentMillis;
    publicarDadosMQTT(estados);

    // Controle dos LEDs conforme os níveis de CO₂
    if ((CO2 < 650) && (manual==false)){
      digitalWrite(LED_GREEN, LOW);
      digitalWrite(LED_YELLOW, LOW);
      digitalWrite(LED_RED, LOW);
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

  //enviarComandoSonoff(ips, ids, estados);
  }
}
