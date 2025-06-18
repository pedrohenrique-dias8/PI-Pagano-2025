// Programa: Modulo ESP8266 Serial Output
// Autor: Gabriel Manoel da Silva - DAS/UFSC

#include <Wire.h>
#include <ArduinoJson.h>
#include <Time.h>
#include <ADS1115_WE.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <math.h>

// JSON
JsonDocument doc;

// ===== CONFIGS =====
const int REPORT_INTERVAL = 2000; // a cada 2 segundos
#define I2C_ADDRESS 0x48  
#define D5 14  
#define LED_1 16  
#define Number_of_Channels 3

// ===== CONFIG WIFI =====
const char* ssid = "LMM";
const char* password = "mecatronica";

// ===== CONFIG MQTT =====
const char* mqtt_server = "2d3476b755a14f60ba5c6449a3dab224.s1.eu.hivemq.cloud"; // ou seu domínio HiveMQ
const int mqtt_port = 8883;               // ou 8883 se for TLS
const char* mqtt_topic = "DME_ENTRADA";
const char* mqtt_client_user = "teste123";        // se tiver login no broker
const char* mqtt_client_pass = "Teste123";
WiFiClientSecure espClient;
PubSubClient client(espClient);

// ===== OBJETOS =====
ADS1115_WE adc(I2C_ADDRESS);

unsigned long lastSend = 0;

// ===== CONECTAR WIFI =====
void conectarWiFi() {
  Serial.print("Conectando ao Wi-Fi: ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);
  unsigned long startAttemptTime = millis();

  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 15000) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi conectado!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nFalha ao conectar! Verifique as credenciais.");
  }
}

// ===== CONECTAR MQTT =====
void reconectarMQTT() {
  while (!client.connected()) {
    Serial.print("Conectando ao broker MQTT... ");
    String clientId = "ESP8266Client-" + String(random(0xffff), HEX);
    if (client.connect(clientId.c_str(), mqtt_client_user, mqtt_client_pass)) {
      Serial.println("Conectado com sucesso!");
    } else {
      Serial.print("Falhou. Código: ");
      Serial.print(client.state());
      Serial.println(" Tentando novamente em 5 segundos...");
      delay(5000);
    }
  }
}

// ===== SETUP =====
void setup() {
  Wire.begin();
  Serial.begin(9600);
  pinMode(LED_1, OUTPUT);
  pinMode(D5, OUTPUT);

  if (!adc.init()) {
    Serial.println("ADS1115 não conectado!");
  }

  adc.setVoltageRange_mV(ADS1115_RANGE_1024);
  adc.setConvRate(ADS1115_475_SPS);
  adc.setMeasureMode(ADS1115_CONTINUOUS);

  conectarWiFi();
  client.setServer(mqtt_server, mqtt_port);
  espClient.setInsecure();
}

// ===== LOOP =====
void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi desconectado. Tentando reconectar...");
    conectarWiFi();
    return;
  }

  if (!client.connected()) {
    reconectarMQTT();
  }

  client.loop();

  if (millis() - lastSend > REPORT_INTERVAL) {
    lastSend = millis();
    getAndSendData();
    digitalWrite(LED_1, !digitalRead(LED_1));
  }
}

// ===== CÁLCULO DE CORRENTE RMS =====
double calcIrms(unsigned int samples, ADS1115_MUX canal, int index) {
  double sqI = 0;
  adc.setCompareChannels(canal);
  for (unsigned int n = 0; n < samples; n++) {
    int sample = adc.getResult_mV();
    sqI += sample * sample;
    digitalWrite(D5, !digitalRead(D5));
  }

  double Irms = sqrt(sqI / samples);
  if (index == 1) Irms *= (2 / 15.4);
  if (index == 2) Irms *= (2 / 15.5);
  if (index == 3) Irms *= (2 / 15.2);

  return Irms;
}

// ===== COLETA E ENVIO =====
void getAndSendData() {
  float IRMS1 = calcIrms(1000, ADS1115_COMP_0_3, 1);
  float IRMS2 = calcIrms(1000, ADS1115_COMP_1_3, 2);
  float IRMS3 = calcIrms(1000, ADS1115_COMP_2_3, 3);

  Serial.printf("IRMS 1: %.2f | IRMS 2: %.2f | IRMS 3: %.2f\n", IRMS1, IRMS2, IRMS3);

  String dados = "{\"nome\": \"LMM_entrada\", \"corrente\": [" +
                 String(IRMS1, 2) + ", " +
                 String(IRMS2, 2) + ", " +
                 String(IRMS3, 2) + "]}";

  client.publish(mqtt_topic, dados.c_str());
}
