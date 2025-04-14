// Programa: Modulo ESP8266 Serial Output
// Autor : Gabriel Manoel da Silva - DAS/UFSC  

#include <Wire.h>
#include <ArduinoJson.h>
#include <Time.h>
#include <ADS1115_WE.h>
#include <ESP8266WiFi.h>  // 🔄 Biblioteca correta para WiFi no ESP8266
//#include <WiFi.h>
#include <PubSubClient.h>
//#include <SoftwareSerial.h>  // 🔄 Para comunicação serial extra
JsonDocument doc;
const int REPORT_INTERVAL = 2 * 1000; // a cada 30 s  

// Endereço do ADS1115  
#define I2C_ADDRESS 0x48  
#define D5 14  
#define LED_1 16  
#define Number_of_Channels 3


// ===== CONFIGURAÇÃO DO WIFI =====
const char* ssid = "LMM";         // 🔴 Nome da rede WiFi
const char* password = "mecatronica"; // 🔴 Senha da rede WiFi

// ===== CONFIGURAÇÃO DO MQTT =====
const char* mqtt_server = "192.168.0.142";
const int mqtt_port = 1883;
const char* mqtt_topic = "energia_entrada";
const char* mqtt_client_id = "ESP8266_CM002";

WiFiClient espClient;
PubSubClient client(espClient);

ADS1115_WE adc(I2C_ADDRESS);

unsigned long lastSend;

void setup() {
    Wire.begin();
    pinMode(LED_1, OUTPUT);
    
    if (!adc.init()) {
        Serial.println("ADS1115 não conectado!");
    }
    
    adc.setVoltageRange_mV(ADS1115_RANGE_1024);
    adc.setConvRate(ADS1115_475_SPS);
    adc.setMeasureMode(ADS1115_CONTINUOUS);
    
    Serial.begin(9600);  // Serial principal
  

    conectarWiFi();
    client.setServer(mqtt_server, mqtt_port);
}

void loop() {
    client.loop();
    if (!client.connected()) {
        reconectarMQTT();
    }
    if (millis() - lastSend > REPORT_INTERVAL) {
        getAndSendData();
        lastSend = millis();
        digitalWrite(LED_1, !digitalRead(LED_1));
    }

    // Publica no MQTT
//    String msg = String(payload);
  //  client.publish(mqtt_topic, msg.c_str());

    delay(5000);
}

double calcIrms(unsigned int Number_of_Samples, ADS1115_MUX channel, int canal) {
    double sqI = 0;
    double sumI = 0;
    
    adc.setCompareChannels(channel);
    for (unsigned int n = 0; n < Number_of_Samples; n++) {
        int sampleI = adc.getResult_mV();
        //Serial.println(sampleI);
        sqI = sampleI * sampleI;
        sumI += sqI;
        digitalWrite(D5, !digitalRead(D5));
    }
    //channel["resistencia"];
    double Irms = sqrt(sumI / Number_of_Samples);
    if (canal == 1) Irms *= (2 / 15.4);
    if (canal == 2) Irms *= (2 / 15.5);
    if (canal == 3) Irms *= (2 / 15.2);
    
    return Irms;
}

void getAndSendData() {
    for (unsigned int n = 0; n < Number_of_Channels; n++) {
    }
    String payload = "";
    float IRMS1 = calcIrms(1000, ADS1115_COMP_0_3, 1);
    Serial.print("IRMS 1:");
    Serial.println(IRMS1);
    
    float IRMS2 = calcIrms(1000, ADS1115_COMP_1_3, 2);
    Serial.print("IRMS 2:");
    Serial.println(IRMS2);
    
    float IRMS3 = calcIrms(1000, ADS1115_COMP_2_3, 3);
    Serial.print("IRMS 3:");
    Serial.println(IRMS3);
    //doc[""]
    //String payload = "{";
    payload += "{corrente_1:"; payload += IRMS1; payload += ",";
    payload += "corrente_2:"; payload += IRMS2; payload += ",";
    payload += "corrente_3:"; payload += IRMS3; payload += "}";
    float var_correntes[3] = {IRMS1, IRMS2, IRMS3};
    //dado = "{\"nome\": \"LMM_entrada\", \"corrente\":" + var_correntes + "}";
    String dados = "{\"nome\": \"LMM_entrada\", \"corrente\": [" + 
          String(IRMS1, 2) + ", " + 
          String(IRMS2, 2) + ", " + 
          String(IRMS3, 2) + "]}";

    // Envia dados via MQTT
    client.publish(mqtt_topic, dados.c_str());
}

void conectarWiFi() {
    Serial.print("Conectando ao WiFi");
    WiFi.begin(ssid, password);
    
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println("\nWiFi Conectado!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
}

void reconectarMQTT() {
    while (!client.connected()) {
        Serial.print("Conectando ao MQTT...");
        if (client.connect(mqtt_client_id)) {
            Serial.println("Conectado!");
        } else {
            Serial.print("Falha. Código: ");
            Serial.println(client.state());
            delay(2000);
        }
    }
}

// string({"nome": "LMM_entrada", "corrente": [corrente1, corrente2, corrente3]})
// {"nome": "LMM_fundo", "corrente": [corrente1, corrente2, corrente3], "tensao": [tensao1, tensao2, tensao3]}
