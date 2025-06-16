#include <ArduinoJson.h>
#include <TimeLib.h>
#include "EmonLib.h"
#include <WiFi.h>
#include <PubSubClient.h>

// ======== CONFIG WIFI ========
const char* ssid = "LMM";
const char* password = "mecatronica";

// ======== CONFIG MQTT ========
const char* mqtt_server = "192.168.0.142";
const int mqtt_port = 1883;
const char* mqtt_topic = "energia_fundo";
const char* mqtt_client_id = "ESP32_LMM_FUNDO";

WiFiClient espClient;
PubSubClient client(espClient);

// ========== PINOS ==========
#define InputSCT_3 36
#define InputSCT_2 39
#define InputSCT_1 34
#define InputSVT_1 35
#define InputSVT_2 33
#define InputSVT_3 32

int LED_BUILTIN = 2;

// ========== CALIBRAÇÕES ==========
#define V_calibration_1  202       
#define V_calibration_2  202        
#define V_calibration_3  202        
#define I_calibration    42.5       

// ========== VARIÁVEIS ==========
double Irms_value_1 = 0, Irms_value_2 = 0, Irms_value_3 = 0;
double Vrms_value_1 = 0, Vrms_value_2 = 0, Vrms_value_3 = 0;
double Preal_value_1 = 0, Preal_value_2 = 0, Preal_value_3 = 0;

unsigned long lastSend = 0;

EnergyMonitor EnerMonitor;
EnergyMonitor EnerMonitor_2;
EnergyMonitor EnerMonitor_3;

// ========== FUNÇÕES DE CONEXÃO ==========
void conectarWiFi() {
  Serial.print("Conectando ao WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi conectado!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}

void reconectarMQTT() {
  while (!client.connected()) {
    Serial.print("Conectando ao broker MQTT...");
    if (client.connect(mqtt_client_id)) {
      Serial.println("MQTT conectado!");
    } else {
      Serial.print("Falhou! Código: ");
      Serial.println(client.state());
      delay(2000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  analogReadResolution(12);

  Serial.println("Iniciando monitoramento...");

  // Setup dos sensores
  EnerMonitor.current(InputSCT_1, I_calibration);  
  EnerMonitor_2.current(InputSCT_2, I_calibration);
  EnerMonitor_3.current(InputSCT_3, I_calibration);

  EnerMonitor.voltage(InputSVT_1, V_calibration_1, 0);
  EnerMonitor_2.voltage(InputSVT_2, V_calibration_2, 0);
  EnerMonitor_3.voltage(InputSVT_3, V_calibration_3, 0);

  // Conectar ao WiFi e configurar MQTT
  conectarWiFi();
  client.setServer(mqtt_server, mqtt_port);
}

void loop() {
  digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
  
  client.loop();
  if (!client.connected()) {
    reconectarMQTT();
  }

  if (millis() - lastSend > 10000) {
    getAndSendData();
    lastSend = millis();
  }
}

void getAndSendData() {
  // Medição
  EnerMonitor.calcVI(60, 5000);
  EnerMonitor_2.calcVI(60, 5000);
  EnerMonitor_3.calcVI(60, 5000);

  Vrms_value_1 = EnerMonitor.Vrms;
  Irms_value_1 = (EnerMonitor.Irms >= 0.5) ? EnerMonitor.Irms : 0;
  Preal_value_1 = (abs(EnerMonitor.realPower) >= 50) ? abs(EnerMonitor.realPower) : 0;

  Vrms_value_2 = EnerMonitor_2.Vrms;
  Irms_value_2 = (EnerMonitor_2.Irms >= 0.5) ? EnerMonitor_2.Irms : 0;
  Preal_value_2 = (abs(EnerMonitor_2.realPower) >= 50) ? abs(EnerMonitor_2.realPower) : 0;

  Vrms_value_3 = EnerMonitor_3.Vrms;
  Irms_value_3 = (EnerMonitor_3.Irms >= 0.5) ? EnerMonitor_3.Irms : 0;
  Preal_value_3 = (abs(EnerMonitor_3.realPower) >= 50) ? abs(EnerMonitor_3.realPower) : 0;

  Serial.println("Enviando dados via MQTT...");

  // Monta JSON no padrão desejado
  String payload = "{\"nome\": \"LMM_fundo\", \"corrente\": [" +
    String(Irms_value_1, 2) + ", " +
    String(Irms_value_2, 2) + ", " +
    String(Irms_value_3, 2) + "], \"tensao\": [" +
    String(Vrms_value_1, 2) + ", " +
    String(Vrms_value_2, 2) + ", " +
    String(Vrms_value_3, 2) + "], \"potencia\": [" +
    String(Preal_value_1, 2) + ", " +
    String(Preal_value_2, 2) + ", " +
    String(Preal_value_3, 2) + "]}";

  client.publish(mqtt_topic, payload.c_str());

  // Log no serial também
  Serial.println(payload);
}
