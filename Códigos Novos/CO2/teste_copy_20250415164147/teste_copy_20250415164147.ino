#include <WiFi.h>
#include <HTTPClient.h>

const char* ssid = "LCA";
const char* password = "12345678";

String enderecoIp = "192.168.1.103"; // Ex: "192.168.0.10"
String idSonoff = "ESP_B2492D"; // Ex: "1000abcdef"

int statusSonoff = -1; // -1 = indefinido, 0 = off, 1 = on

void setup() {
  Serial.begin(115200);
  delay(4000);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Conectando a rede WiFi...");
  }
  Serial.println("Conectado!");
  Serial.println("Envie 1 para ligar ou 0 para desligar o relé.");
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Erro na conexão WiFi!");
    delay(1000);
    return;
  }

  if (Serial.available()) {
    String comando = Serial.readStringUntil('\n');
    comando.trim(); // Remove espaços e quebras de linha extras

    if (comando == "1" && statusSonoff != 1) {
      enviarComandoSonoff("on");
      statusSonoff = 1;
      Serial.println("Relé LIGADO.");
    } 
    else if (comando == "0" && statusSonoff != 0) {
      enviarComandoSonoff("off");
      statusSonoff = 0;
      Serial.println("Relé DESLIGADO.");
    }
  }
}

void enviarComandoSonoff(String estado) {
  HTTPClient http;
  String url = "http://" + enderecoIp + ":8081/zeroconf/switch";
  http.begin(url);
  http.addHeader("Content-Type", "application/json");

  String corpo = "{\"deviceid\":\"" + idSonoff + "\",\"data\":{\"switch\":\"" + estado + "\"}}";
  int httpCode = http.POST(corpo);

  if (httpCode > 0) {
    Serial.println("Comando enviado com sucesso: " + estado);
  } else {
    Serial.println("Erro ao enviar comando: " + http.errorToString(httpCode));
  }

  http.end();
}
