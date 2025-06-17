#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <DHT.h>

// ====== Konfigurasi WiFi dan MQTT ThingsBoard ======
const char* ssid = "Delnafa Cake";
const char* password = "1020304050";
const char* mqttServer = "thingsboard.cloud";
const char* token = "7MCU5sh6Stg9b1DjBmO2";  // Token dari perangkat ThingsBoard

WiFiClient espClient;
PubSubClient client(espClient);
AsyncWebServer server(80);

// ====== Konfigurasi Sensor ESP1 ======
#define DHTPIN 21
#define DHTTYPE DHT21
#define SOIL_PIN 34
DHT dht(DHTPIN, DHTTYPE);

// ====== Variabel Data dari ESP2 ======
float esp2_temp = 0;
float esp2_humid = 0;
float esp2_soil = 0;
bool esp2_data_received = false;
unsigned long lastESP2Receive = 0;

// ====== Interval kirim ThingsBoard ======
unsigned long lastSend = 0;
const unsigned long interval = 30000; // 30 detik

// ====== Fungsi Koneksi WiFi ======
void connectWiFi() {
  WiFi.begin(ssid, password);
  Serial.print("📶 Menghubungkan ke WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
  }
  Serial.println("\n✅ Terhubung ke WiFi");
  Serial.print("IP ESP1: "); Serial.println(WiFi.localIP());
}

// ====== Fungsi MQTT ======
void connectMQTT() {
  while (!client.connected()) {
    Serial.print("🔁 Menghubungkan ke ThingsBoard...");
    if (client.connect("ESP1", token, "")) {
      Serial.println("✅ Terhubung ke ThingsBoard");
    } else {
      Serial.print("❌ Gagal: ");
      Serial.println(client.state());
      delay(2000);
    }
  }
}

// ====== Web Server untuk Terima JSON dari ESP2 ======
void setupServer() {
  server.on("/data", HTTP_POST, [](AsyncWebServerRequest *request) {},
    NULL,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t, size_t) {
      DynamicJsonDocument doc(256);
      DeserializationError err = deserializeJson(doc, data);
      if (err) {
        Serial.println("❌ Gagal parsing JSON dari ESP2");
        request->send(400, "application/json", "{\"status\":\"error\"}");
        return;
      }

      esp2_temp = doc["temperature"];
      esp2_humid = doc["humidity"];
      esp2_soil = doc["soilMoisture"];
      esp2_data_received = true;
      lastESP2Receive = millis();

      Serial.println("📥 Data diterima dari ESP2 (192.168.189.197):");
      Serial.print("🌡 Suhu: "); Serial.print(esp2_temp); Serial.println(" °C");
      Serial.print("💧 Kelembaban: "); Serial.print(esp2_humid); Serial.println(" %");
      Serial.print("🌱 Kelembaban Tanah: "); Serial.print(esp2_soil); Serial.println(" %");

      request->send(200, "application/json", "{\"status\":\"received\"}");
  });

  server.begin();
}

void setup() {
  Serial.begin(115200);
  dht.begin();

  connectWiFi();
  client.setServer(mqttServer, 1883);
  setupServer();
}

void loop() {
  if (!client.connected()) {
    connectMQTT();
  }
  client.loop();

  unsigned long now = millis();
  if ((unsigned long)(now - lastSend) >= interval) {
    lastSend = now;

    float temp1 = dht.readTemperature();
    float humid1 = dht.readHumidity();
    int soilRaw = analogRead(SOIL_PIN);
    float soil1 = map(soilRaw, 0, 4095, 0, 100);

    if (isnan(temp1) || isnan(humid1)) {
      Serial.println("❌ Gagal membaca sensor lokal ESP1");
      return;
    }

    DynamicJsonDocument payload(512);
    payload["esp1_temperature"] = temp1;
    payload["esp1_humidity"] = humid1;
    payload["esp1_soil"] = soil1;

    if (esp2_data_received) {
      payload["esp2_temperature"] = esp2_temp;
      payload["esp2_humidity"] = esp2_humid;
      payload["esp2_soil"] = esp2_soil;
    } else {
      payload["esp2_status"] = "❌ Belum ada data dari ESP2";
    }

    // Serialize dan kirim
    char buffer[512];
    serializeJson(payload, buffer);
    client.publish("v1/devices/me/telemetry", buffer);

    // ====== Log Serial Monitor ======
    Serial.println("📤 Mengirim data ke ThingsBoard:");
    Serial.print("ESP1 => 🌡 "); Serial.print(temp1); Serial.print("°C, 💧 ");
    Serial.print(humid1); Serial.print("%, 🌱 "); Serial.print(soil1); Serial.println("%");

    if (esp2_data_received) {
      Serial.print("ESP2 => 🌡 "); Serial.print(esp2_temp); Serial.print("°C, 💧 ");
      Serial.print(esp2_humid); Serial.print("%, 🌱 "); Serial.print(esp2_soil); Serial.println("%");
    } else {
      Serial.println("⚠️ Belum ada data masuk dari ESP2");
    }

    Serial.println("==================================================");
  }
}