#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>

// ======================================================
// CONFIGURACIÓN WIFI
// ======================================================
const char* WIFI_SSID = "iPhone de Julio";
const char* WIFI_PASSWORD = "hola1234";

// ======================================================
// CONFIGURACIÓN MQTT
// ======================================================
const char* MQTT_SERVER = "172.20.10.3"; 
const int MQTT_PORT = 1883;

// Topics utilizados por el dashboard React
const char* TOPIC_TEMPERATURA = "clima/esp32/temperatura";
const char* TOPIC_HUMEDAD = "clima/esp32/humedad";
const char* TOPIC_LUZ = "clima/esp32/luz";

// ======================================================
// CONFIGURACIÓN DE SENSORES
// ======================================================
#define DHTPIN 4         // Pin digital 
#define DHTTYPE DHT11    
#define LDR_PIN 32       // Pin analógico 

DHT dht(DHTPIN, DHTTYPE);
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

// Enviar una lectura cada 2 segundos
const unsigned long INTERVALO_ENVIO = 2000;
unsigned long ultimoEnvio = 0;

void conectarWiFi() {
  Serial.print("Conectando a WiFi");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi conectado");
  Serial.print("IP del ESP32: ");
  Serial.println(WiFi.localIP());
}

void conectarMQTT() {
  while (!mqttClient.connected()) {
    Serial.print("Conectando a Mosquitto...");

    String clientId = "ESP32-";
    clientId += String((uint32_t)(ESP.getEfuseMac() & 0xFFFFFFFF), HEX);

    if (mqttClient.connect(clientId.c_str())) {
      Serial.println(" conectado");
    } else {
      Serial.print(" error, rc=");
      Serial.print(mqttClient.state());
      Serial.println(". Reintentando en 2 segundos...");
      delay(2000);
    }
  }
}


void publicarSensores() {
  // 1. Obtener la temperatura y la humedad
  float temperatura = dht.readTemperature();
  float humedad = dht.readHumidity();

  // Validar lectura correcta del DHT
  if (isnan(temperatura) || isnan(humedad)) {
    Serial.println("Error al leer el sensor DHT. Revisa las conexiones.");
    return;
  }

  // 2. Obtener la lectura analógica del LDR (0 a 4095) y convertir a porcentaje (0 a 100)
  int adcVal = analogRead(LDR_PIN);
  int luzPorcentaje = map(adcVal, 0, 4095, 0, 100);
  luzPorcentaje = constrain(luzPorcentaje, 0, 100);

  // 3. Preparar los valores para publicarlos por MQTT
  char tempStr[10];
  char humStr[10];
  char luzStr[10];

  dtostrf(temperatura, 1, 2, tempStr);
  dtostrf(humedad, 1, 2, humStr);
  snprintf(luzStr, sizeof(luzStr), "%d", luzPorcentaje);

  // 4. Publicar cada dato en su topic correspondiente
  mqttClient.publish(TOPIC_TEMPERATURA, tempStr);
  mqttClient.publish(TOPIC_HUMEDAD, humStr);
  mqttClient.publish(TOPIC_LUZ, luzStr);

  // 5. Mostrar las mediciones en el Monitor Serial
  Serial.print("Temperatura: ");
  Serial.print(temperatura, 2);
  Serial.println(" °C");

  Serial.print("Humedad: ");
  Serial.print(humedad, 2);
  Serial.println(" %");

  Serial.print("LDR ADC: ");
  Serial.print(adcVal);
  Serial.print(" | Luz: ");
  Serial.print(luzPorcentaje);
  Serial.println(" %");

  Serial.println("------------------------------");
}

void setup() {
  Serial.begin(115200);

  // Configuración de pines e inicialización del DHT
  pinMode(LDR_PIN, INPUT);
  dht.begin();

  conectarWiFi();
  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
}

void loop() {
  // Reconexión automática de WiFi si se pierde la señal
  if (WiFi.status() != WL_CONNECTED) {
    conectarWiFi();
  }

  // Reconexión automática a Mosquitto si cae la conexión
  if (!mqttClient.connected()) {
    conectarMQTT();
  }

  mqttClient.loop();

  // Publicar mediciones de los sensores cada 2 segundos
  if (millis() - ultimoEnvio >= INTERVALO_ENVIO) {
    ultimoEnvio = millis();
    publicarSensores();
  }
}