#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>

const char* ssid = "stc_wifi_8CF1";
const char* password = "5ZM6G24QB2";

const char* mqtt_server = "broker.hivemq.com";
const int mqtt_port = 1883;

#define DHTPIN 2
#define DHTTYPE DHT22
#define GAS_SENSOR_PIN A3

#define BUZZER_PIN 5  
#define LED_PIN 6     

DHT dht(DHTPIN, DHTTYPE);
WiFiClient espClient;
PubSubClient client(espClient);

bool manualOverride = false; 

void setup_wifi() {
  Serial.begin(115200);
  delay(10);
  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ WiFi connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* message, unsigned int length) {
  String msg = "";
  for (int i = 0; i < length; i++) {
    msg += (char)message[i];
  }

  Serial.print(" Message received [");
  Serial.print(topic);
  Serial.print("]: ");
  Serial.println(msg);

  if (String(topic) == "iot/control/buzzer") {
    if (msg == "off") {
      manualOverride = true;
      digitalWrite(BUZZER_PIN, LOW);
      Serial.println("🔕 Buzzer OFF (manual override)");
    } else if (msg == "on") {
      manualOverride = false;
      Serial.println("🔔 Manual override cleared");
    }
  }
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Connecting to MQTT...");
    if (client.connect("NanoESP32Client")) {
      Serial.println("connected!");
      client.subscribe("iot/control/buzzer");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" retrying in 5s...");
      delay(5000);
    }
  }
}

void setup() {
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  dht.begin();
  delay(2000);

  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  float temp = dht.readTemperature();
  float hum = dht.readHumidity();

  // Read gas sensor 10 times and average
  long sum = 0;
  for (int i = 0; i < 10; i++) {
    sum += analogRead(GAS_SENSOR_PIN);
    delay(20);
  }
  int gasValue = sum / 10;

  // Display values
  if (isnan(temp) || isnan(hum)) {
    Serial.println(" DHT read failed!");
    return;
  }

  Serial.println(" Sensor Readings:");
  Serial.print("🌡 Temp: "); Serial.print(temp); Serial.println("°C");
  Serial.print("💧 Humidity: "); Serial.print(hum); Serial.println("%");
  Serial.print("🔥 Gas Level: "); Serial.println(gasValue);

  // Alert condition (unless manually silenced)
  if (!manualOverride) {
    if (gasValue >= 720) {
      digitalWrite(BUZZER_PIN, HIGH);
      digitalWrite(LED_PIN, HIGH);
      Serial.println("🚨 Gas too high! ALERT ON");
    } else {
      digitalWrite(BUZZER_PIN, LOW);
      digitalWrite(LED_PIN, LOW);
    }
  } else {
    Serial.println(" Manual override active, no alert triggered");
  }

  // Send values to MQTT topics
  client.publish("iot/sensor/temp", String(temp).c_str());
  client.publish("iot/sensor/humidity", String(hum).c_str());
  client.publish("iot/sensor/gas", String(gasValue).c_str());

  Serial.println("---------------------------\n");
  delay(5000); // Delay between readings
}

