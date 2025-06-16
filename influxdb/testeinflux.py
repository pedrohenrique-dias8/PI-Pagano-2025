import paho.mqtt.client as mqtt
from influxdb_client import InfluxDBClient, Point
from influxdb_client.client.write_api import SYNCHRONOUS
import os

# Configuração do MQTT
MQTT_BROKER = "2a7050fcee684f8ab8199d7997a90fa2.s1.eu.hivemq.cloud"  # IP do servidor Mosquitto
HIVEMQ_USERNAME = "teste123"  # Nome de usuário do HiveMQ
HIVEMQ_PASSWORD = "Teste123"  # Senha do HiveMQ
MQTT_PORT = 8883
MQTT_TOPIC = "CO2-POSAUTO/CO2"
with open(".env.influxdb2-admin-token", "r") as token_file:
    os.environ["INFLUXDB_TOKEN"] = token_file.read()

# Configuração do InfluxDB
INFLUX_URL = "http://172.19.0.4:8086"  # Certifique-se de que o endereço esteja correto
INFLUX_TOKEN = os.environ.get("INFLUXDB_TOKEN")  # Certifique-se de que o token esteja correto
INFLUX_ORG = "PIPAGANO"  # Nome da organização configurada no InfluxDB
INFLUX_BUCKET = "home"  # Nome do bucket configurado no InfluxDB
print(INFLUX_TOKEN)
# Conectar ao InfluxDB
client = InfluxDBClient(url=INFLUX_URL, token=INFLUX_TOKEN, org=INFLUX_ORG)
write_api = client.write_api(write_options=SYNCHRONOUS)
query_api = client.query_api()  # Para fazer consultas (se necessário)

# Função chamada quando uma mensagem MQTT é recebida
def on_message(client, userdata, msg):
    try:
        payload = msg.payload.decode("utf-8")
        print(f"Recebido: {payload}")

        # Criando o ponto de dados para o InfluxDB
        point = Point("sensor") \
            .tag("tipo", "CO2") \
            .field("valor", float(payload))  # O valor do CO2

        # Exibindo no formato de linha do InfluxDB (para depuração)
        print(f"Enviando para InfluxDB: {point.to_line_protocol()}")

        # Escrever o ponto de dados no InfluxDB
        write_api.write(bucket=INFLUX_BUCKET, org=INFLUX_ORG, record=point)
        print("✅ Dado enviado para o InfluxDB")

    except Exception as e:
        print(f"❌ Erro ao enviar: {e}")


# Configurar o cliente MQTT
mqtt_client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
mqtt_client.on_message = on_message
mqtt_client.username_pw_set(HIVEMQ_USERNAME, HIVEMQ_PASSWORD)  # Configurar usuário e senha
mqtt_client.tls_set(tls_version=mqtt.ssl.PROTOCOL_TLS)
mqtt_client.connect(MQTT_BROKER, MQTT_PORT)

# Inscrever-se no tópico
mqtt_client.subscribe(MQTT_TOPIC)
print(f"Escutando no tópico: {MQTT_TOPIC}")

# Iniciar o loop MQTT
mqtt_client.loop_forever()
