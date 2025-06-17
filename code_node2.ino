#include <WiFi.h>
#include <HTTPClient.h>
#include <DHT.h>
#include <ArduinoJson.h>

// ====== Konfigurasi WiFi dan IP Server (ESP1) ======
const char* ssid = "Delnafa Cake";
const char* password = "1020304050";
const char* serverIP = "192.168.189.133"; // IP ESP1

// ====== Konfigurasi Sensor ======
#define DHTPIN 21
#define DHTTYPE DHT21
#define SOIL_PIN 34

DHT dht(DHTPIN, DHTTYPE);

// ====== Timer Interval (30 Detik) ======
unsigned long lastSend = 0;
const unsigned long interval = 30000; // 30 detik

void setup() {
  Serial.begin(115200);
  dht.begin();

  WiFi.begin(ssid, password);
  Serial.print("📶 Menghubungkan ke WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
  }
  Serial.println("\n✅ Terhubung ke WiFi!");
  Serial.print("IP ESP2: "); Serial.println(WiFi.localIP());
}

void loop() {
  unsigned long now = millis();
  if ((unsigned long)(now - lastSend) >= interval) {
    lastSend = now;

    // ====== Baca Sensor ======
    float temp = dht.readTemperature();
    float humid = dht.readHumidity();
    int soilRaw = analogRead(SOIL_PIN);
    float soilPercent = map(soilRaw, 0, 4095, 0, 100);

    if (isnan(temp) || isnan(humid)) {
      Serial.println("❌ Gagal membaca sensor DHT!");
      return;
    }

    // ====== Kirim Data ke ESP1 via HTTP POST ======
    if (WiFi.status() == WL_CONNECTED) {
      HTTPClient http;
      String url = "http://" + String(serverIP) + "/data";

      // 🐞 DEBUG: Cek URL dan http.begin()
      Serial.print("🔗 Mengakses: ");
      Serial.println(url);
      
      bool ok = http.begin(url);
      if (!ok) {
        Serial.println("❌ http.begin() gagal! Periksa IP dan server ESP1.");
        return;
      }

      http.addHeader("Content-Type", "application/json");

      // Buat JSON Payload
      DynamicJsonDocument doc(256);
      doc["temperature"] = temp;
      doc["humidity"] = humid;
      doc["soilMoisture"] = soilPercent;

      String jsonString;
      serializeJson(doc, jsonString);

      int httpResponseCode = http.POST(jsonString);
      if (httpResponseCode > 0) {
        Serial.println("✅ Data terkirim ke ESP1!");
        Serial.println("📨 Respon: " + http.getString());
      } else {
        Serial.print("❌ Gagal kirim HTTP. Kode: ");
        Serial.println(httpResponseCode);
      }

      http.end();
    } else {
      Serial.println("⚠ WiFi terputus!");
    }

    // === Log Serial Tiap 30 Detik ===
    Serial.println("📊 Log Sensor:");
    Serial.print("🌡 Suhu: "); Serial.print(temp); Serial.println(" °C");
    Serial.print("💧 Kelembaban: "); Serial.print(humid); Serial.println(" %");
    Serial.print("🌱 Kelembaban Tanah: "); Serial.print(soilPercent); Serial.println(" %");
    Serial.println("--------------------------------------------------");
  }
}