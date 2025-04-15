//////////// Inicialização de bibliotecas //////////// 

#include <IRremote.h>
#include "DHT.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include <WebServer.h>
#include <Preferences.h>

//////////// Definição das variáveis //////////// 

#define IR_PIN 4          // Pino do led IR
#define PIR_PIN 12        // Pino do sensor de presença 
#define DHTPIN 5          // Pino do DHT11
#define DHTTYPE DHT11     // Modelo do sensor

Preferences preferences;  // Armazena credenciais Wi-Fi na memória flash

// Configurações MQTT
const char* mqtt_server = "192.168.0.142"; // IP do Broker Mosquitto
WiFiClient espClient;
PubSubClient client(espClient);

String ssid;
String password;

WebServer server(80);

DHT dht(DHTPIN, DHTTYPE);
IRsend irsend;

uint16_t powerOn[] = {2886, 1854,  380, 1166,  354, 1192,  354, 470,  332, 494,  380, 446,  380, 1164,  380, 444,  380, 460,  382, 1164,  380, 1166,  404, 422,  380, 1164,  406, 420,  404, 420,  404, 1142,  382, 1178,  406, 420,  404, 1142,  404, 1142,  406, 418,  382, 444,  380, 1166,  382, 444,  382, 458,  380, 1164,  380, 444,  380, 444,  380, 444,  380, 444,  356, 470,  380, 444,  380, 460,  382, 442,  378, 446,  380, 444,  380, 446,  380, 444,  380, 446,  404, 420,  380, 458,  406, 418,  380, 444,  406, 1140,  382, 444,  378, 446,  380, 1164,  382, 444,  380, 458,  382, 1164,  406, 1142,  382, 444,  380, 444,  380, 444,  380, 446,  380, 446,  380, 458,  382, 444,  380, 446,  380, 444,  380, 1166,  406, 420,  378, 446,  378, 446,  380, 458,  380, 1166,  406, 418,  380, 1166,  404, 420,  404, 420,  380, 444,  380, 1164,  406, 434,  406, 420,  380, 446,  380, 444,  404, 446,  354, 444,  402, 422,  378, 470,  354, 460,  380, 448,  400, 424,  378, 470,  354, 444,  380, 446,  378, 446,  378, 470,  354, 460,  380, 448,  378, 446,  378, 448,  376, 446,  378, 470,  354, 446,  380, 446,  380, 458,  380, 470,  354, 470,  354, 470,  352, 472,  354, 470,  354, 470,  354, 470,  354, 462,  378, 1166,  380, 470,  354, 472,  354, 1168,  380, 470,  354, 472,  352, 448,  378, 1184,  378};  // UNKNOWN C8080FBE

uint16_t set_temp_21[] = {2886, 1856,  380, 1140,  380, 1166,  402, 446,  356, 468,  356, 470,  356, 1190,  356, 470,  354, 460,  382, 1164,  382, 1164,  380, 470,  356, 1166,  382, 468,  332, 494,  358, 1164,  382, 1204,  356, 468,  382, 1138,  408, 1164,  382, 444,  382, 442,  382, 1138,  434, 416,  382, 456,  408, 1112,  408, 442,  408, 416,  408, 418,  408, 416,  408, 416,  408, 416,  408, 430,  408, 390,  408, 442,  408, 416,  408, 418,  408, 416,  406, 420,  406, 418,  408, 432,  384, 440,  406, 418,  384, 1136,  410, 440,  408, 418,  406, 1114,  410, 442,  384, 456,  408, 1112,  410, 1162,  384, 442,  384, 442,  382, 442,  384, 440,  384, 440,  384, 456,  384, 440,  382, 1138,  410, 440,  384, 1138,  410, 440,  382, 442,  384, 442,  382, 456,  382, 1162,  384, 442,  382, 1162,  384, 442,  382, 442,  382, 442,  384, 442,  382, 458,  382, 442,  382, 442,  382, 442,  384, 442,  382, 442,  384, 442,  384, 440,  382, 458,  382, 442,  380, 444,  380, 444,  382, 444,  382, 444,  378, 444,  380, 444,  380, 460,  380, 446,  354, 470,  356, 468,  354, 470,  354, 470,  352, 472,  354, 470,  352, 486,  354, 472,  352, 472,  352, 472,  352, 472,  352, 472,  352, 472,  352, 474,  352, 486,  354, 1192,  352, 1194,  378, 448,  378, 1166,  378, 448,  400, 424,  378, 1168,  400, 440,  378};  // UNKNOWN F471A71D

uint16_t ligarAr[] = {2908, 1834,  382, 1164,  382, 1164,  382, 442,  380, 444,  380, 444,  380, 1136,  410, 444,  380, 458,  380, 1140,  406, 1164,  382, 444,  380, 1138,  408, 442,  380, 444,  382, 1166,  380, 1180,  382, 442,  382, 1164,  382, 1138,  408, 442,  382, 420,  406, 1138,  408, 418,  406, 432,  406, 1138,  408, 418,  406, 418,  406, 418,  382, 444,  406, 418,  406, 418,  432, 408,  382, 444,  406, 418,  432, 392,  382, 444,  432, 392,  432, 392,  458, 366,  432, 408,  408, 418,  456, 368,  432, 1114,  434, 392,  382, 442,  434, 1086,  484, 366,  382, 458,  432, 1084,  462, 1112,  434, 390,  382, 444,  408, 418,  406, 418,  406, 418,  406, 432,  406, 1140,  432, 392,  408, 416,  408, 1140,  432, 392,  408, 416,  432, 394,  406, 432,  432, 1116,  432, 392,  430, 1086,  460, 392,  432, 392,  406, 418,  408, 418,  406, 432,  408, 418,  406, 418,  408, 418,  406, 418,  406, 418,  408, 416,  408, 418,  406, 432,  408, 416,  408, 416,  408, 418,  406, 418,  408, 418,  408, 416,  408, 418,  408, 432,  408, 416,  408, 416,  408, 416,  432, 392,  408, 416,  408, 418,  408, 416,  408, 432,  408, 416,  408, 416,  408, 418,  408, 416,  406, 418,  408, 416,  408, 418,  408, 432,  408, 416,  408, 1138,  432, 392,  408, 1138,  434, 390,  408, 416,  408, 1138,  434, 408,  408};  // TEKNOPOINT

uint16_t set_temp_23[] = {2936, 1804,  384, 1164,  406, 1140,  382, 444,  382, 442,  380, 444,  382, 1164,  382, 444,  404, 432,  382, 1162,  384, 1162,  384, 444,  380, 1162,  384, 442,  382, 442,  382, 1162,  384, 1176,  384, 444,  380, 1162,  408, 1138,  408, 420,  406, 420,  404, 1140,  382, 442,  406, 432,  408, 1138,  408, 418,  406, 418,  408, 414,  408, 418,  408, 442,  384, 418,  408, 432,  408, 414,  408, 418,  406, 418,  408, 416,  408, 414,  410, 416,  408, 416,  408, 432,  408, 414,  410, 416,  410, 1078,  468, 414,  410, 416,  410, 1134,  412, 414,  410, 430,  410, 1134,  412, 1134,  412, 414,  410, 414,  410, 416,  410, 414,  410, 416,  410, 428,  408, 416,  408, 416,  408, 416,  408, 1136,  412, 414,  410, 414,  410, 416,  408, 432,  408, 1136,  410, 416,  408, 1136,  410, 418,  406, 416,  410, 418,  406, 418,  406, 432,  406, 418,  408, 420,  404, 418,  406, 420,  406, 420,  404, 420,  406, 418,  406, 432,  406, 420,  404, 420,  404, 420,  404, 422,  402, 422,  402, 422,  378, 446,  402, 434,  382, 444,  404, 422,  378, 446,  402, 420,  380, 446,  378, 446,  378, 446,  378, 458,  380, 446,  378, 446,  378, 446,  378, 470,  354, 446,  378, 448,  378, 446,  378, 460,  380, 1164,  380, 446,  380, 444,  380, 1166,  380, 446,  380, 446,  378, 1164,  382, 462,  380};  // TEKNOPOINT

uint16_t set_temp_24[] = {2900, 1842,  346, 1174,  372, 1176,  368, 430,  398, 426,  398, 426,  400, 1172,  372, 426,  394, 444,  396, 1200,  346, 1202,  342, 432,  288, 1308,  238, 536,  394, 430,  290, 1308,  238, 1322,  236, 538,  288, 1308,  236, 1308,  238, 536,  290, 536,  288, 1284,  262, 560,  266, 548,  290, 1308,  238, 536,  290, 536,  290, 534,  408, 416,  384, 440,  290, 534,  410, 430,  432, 392,  408, 416,  504, 320,  408, 416,  508, 316,  496, 328,  408, 416,  290, 574,  384, 416,  508, 292,  458, 1142,  262, 508,  560, 266,  338, 1284,  238, 536,  510, 354,  264, 1284,  384, 1162,  336, 488,  480, 316,  314, 512,  288, 534,  536, 288,  290, 574,  388, 1186,  236, 1310,  334, 1188,  382, 440,  264, 536,  534, 288,  438, 388,  412, 476,  360, 1164,  458, 364,  388, 1186,  428, 370,  486, 340,  364, 486,  458, 342,  336, 504,  286, 538,  360, 462,  478, 374,  426, 398,  450, 374,  428, 398,  360, 440,  456, 382,  360, 466,  336, 486,  362, 464,  434, 390,  458, 364,  434, 390,  362, 464,  338, 502,  362, 462,  338, 486,  360, 466,  360, 464,  288, 536,  336, 488,  336, 490,  360, 480,  412, 412,  436, 362,  436, 414,  338, 486,  412, 414,  436, 388,  412, 412,  436, 404,  434, 390,  432, 392,  338, 488,  408, 1140,  432, 390,  434, 390,  434, 1112,  434, 408,  432};  // UNKNOWN D423A106

uint16_t set_temp_25[] = {2962, 1778,  432, 1116,  406, 1136,  408, 444,  382, 442,  380, 444,  380, 1140,  406, 444,  380, 436,  404, 1140,  406, 1140,  406, 444,  380, 1140,  406, 446,  378, 446,  378, 1142,  404, 1156,  406, 446,  354, 1166,  382, 1164,  380, 470,  354, 446,  378, 1166,  380, 470,  354, 460,  380, 1164,  382, 444,  380, 446,  378, 446,  378, 444,  380, 446,  380, 470,  354, 460,  380, 470,  354, 446,  378, 470,  356, 470,  356, 444,  380, 446,  380, 444,  382, 456,  384, 440,  382, 444,  382, 1162,  382, 444,  382, 444,  382, 1162,  384, 444,  380, 456,  382, 1162,  384, 1162,  384, 442,  384, 442,  382, 442,  382, 466,  358, 442,  382, 456,  384, 442,  382, 1162,  384, 1162,  386, 442,  382, 466,  358, 442,  382, 468,  358, 482,  358, 1160,  384, 468,  358, 1106,  438, 468,  358, 468,  356, 468,  358, 468,  358, 458,  384, 466,  356, 442,  382, 442,  382, 468,  356, 446,  380, 468,  356, 468,  356, 458,  382, 444,  380, 444,  380, 444,  380, 470,  356, 468,  356, 468,  356, 468,  356, 460,  382, 468,  356, 444,  380, 446,  378, 446,  380, 470,  354, 470,  354, 470,  354, 458,  382, 468,  354, 470,  354, 470,  354, 470,  354, 472,  352, 472,  352, 472,  354, 460,  378, 1164,  380, 1168,  378, 1168,  378, 470,  352, 472,  354, 470,  352, 1166,  378, 464,  378};  // TEKNOPOINT

uint16_t desligarAr[] =  {2938, 1784,  326, 1218,  328, 1218,  378, 444,  380, 446,  404, 420,  404, 1142,  404, 420,  404, 434,  406, 1140,  430, 1116,  404, 420,  406, 1142,  404, 418,  406, 420,  406, 1140,  406, 1154,  406, 420,  430, 1118,  428, 1116,  406, 420,  404, 420,  406, 1116,  430, 418,  406, 410,  430, 1140,  428, 396,  406, 420,  404, 396,  404, 420,  404, 444,  380, 420,  406, 434,  404, 420,  406, 444,  380, 444,  406, 396,  404, 444,  404, 396,  406, 444,  380, 434,  406, 422,  404, 420,  406, 442,  380, 444,  408, 416,  380, 1140,  432, 418,  406, 408,  430, 1116,  432, 1116,  430, 418,  406, 418,  406, 418,  406, 420,  406, 420,  406, 408,  430, 418,  406, 418,  406, 420,  406, 1140,  406, 418,  406, 418,  406, 418,  406, 432,  406, 1140,  406, 420,  406, 1140,  406, 420,  404, 420,  406, 420,  380, 1166,  406, 434,  380, 444,  406, 418,  382, 444,  404, 420,  380, 444,  380, 444,  382, 444,  380, 460,  380, 446,  380, 446,  378, 444,  380, 446,  380, 446,  378, 446,  380, 444,  378, 460,  380, 446,  404, 420,  378, 446,  380, 446,  380, 444,  378, 472,  354, 446,  378, 458,  354, 472,  352, 496,  354, 472,  354, 448,  376, 472,  328, 472,  378, 472,  352, 486,  328, 1216,  328, 498,  326, 1218,  330, 496,  328, 498,  300, 524,  328, 496,  328, 1210,  352};  // UNKNOWN 2D337BE8


int temperatura_setada = 23;
bool ar_condicionado_ligado = false; 
unsigned long t = millis(); //inicia timer
unsigned long cicle_time = 10000; //10s

//////////// Definição das funções //////////// 

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
    if (String(topic) == "COMANDO-SALA1") {
    }
}

// Função para conectar ao MQTT
void reconnect() {
    while (!client.connected()) {
        Serial.print("Conectando ao broker MQTT... ");
        if (client.connect("ESP32Client")) {
            Serial.println("Conectado!");
            client.subscribe("COMANDO-SALA1"); // Inscreve-se para receber comandos
        } else {
            Serial.print("Falha. Código: ");
            Serial.print(client.state());
            Serial.println(" Tentando novamente em 5 segundos...");
            delay(5000);
        }
    }
}

void changeWiFiCredentials() {
    Serial.println("\nTrocar rede padrão? (s/n) - Aguarde 5s para ignorar.");

    unsigned long startTime = millis();
    String resposta = "";

    // Espera por uma resposta ou timeout de 5 segundos
    while (millis() - startTime < 5000) {
        if (Serial.available()) {
            resposta = Serial.readStringUntil('\n');
            resposta.trim(); // Remove espaços e quebras de linha
            break;
        }
    }

    if (resposta != "s" && resposta != "S") {
        Serial.println("Tempo expirado ou resposta inválida. Mantendo rede atual.");
        return;
    }

    Serial.println("Digite o novo SSID:");
    while (!Serial.available());
    String newSSID = Serial.readStringUntil('\n');
    newSSID.trim();

    Serial.println("Digite a nova senha:");
    while (!Serial.available());
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
//
void setup() {
  Serial.begin(115200); // Inicia a serial para informações local
  IrSender.begin(IR_PIN); // IR PIN como saida
  pinMode(PIR_PIN,INPUT); // Presença como entrada
  dht.begin();            // Inicia sensor de temperatura e umidade

  // Recuperar credenciais Wi-Fi salvas
  preferences.begin("wifi", true);
  ssid = preferences.getString("ssid", "defaultSSID");
  password = preferences.getString("password", "defaultPassword");
  preferences.end();

  // Pergunta se deseja trocar a rede
  changeWiFiCredentials();

  // Conectar ao Wi-Fi
  connectWiFi();

  // Configurar MQTT
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  reconnect(); // Conectar ao broker MQTT
}


unsigned long previousMillis = 0;
const unsigned long interval = 5000; // 5 segundos

void loop() {
  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;

    float temperatura = dht.readTemperature();
    float umidade = dht.readHumidity();
    bool presenca = digitalRead(PIR_PIN);

    Serial.print("Temperatura: ");
    Serial.print(temperatura);
    Serial.print("°C | Umidade: ");
    Serial.print(umidade);
    Serial.print("% | Presença: ");
    Serial.println(presenca);

    // Envia ao broker MQTT
    if (client.connected()) {
      client.publish("sensor/temperatura", String(temperatura).c_str());
      client.publish("sensor/umidade", String(umidade).c_str());
      client.publish("sensor/presenca", String(presenca).c_str());
    } else {
      Serial.println("MQTT desconectado. Tentando reconectar...");
      // Aqui você pode colocar sua função de reconexão, tipo:
      // reconnectMQTT();
    }

    // Se quiser acionar algo com base na presença:
    if (presenca == 1) {
      // IrSender.sendRaw(powerOn, sizeof(powerOn) / sizeof(powerOn[0]), 38); // 38 kHz
    }
  }

  // Mantém a conexão MQTT ativa
  client.loop();
}
