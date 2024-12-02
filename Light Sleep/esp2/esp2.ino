#include <ESP8266WiFi.h>
#include <DHT.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
extern "C" {
  #include "user_interface.h"
}

// Wi-Fi credentials
const char* ssid = "Pranay";
const char* password = "shourya2010";

// DHT configuration for local data aggregation
#define DHTPIN 2         // GPIO2
#define DHTTYPE DHT11    // DHT11 Sensor
DHT dht(DHTPIN, DHTTYPE);

// Server for receiving data from ESP1
ESP8266WebServer server(5000);

// Variables to store aggregated data
float temperatureESP1 = NAN;
float humidityESP1 = NAN;
float temperatureESP2 = NAN;
float humidityESP2 = NAN;

// Raspberry Pi server URL
const char* rpiServerUrl = "http://192.168.127.128:2173/data";

// Sleep duration in microseconds (1 minute)
const uint32_t sleepTimeUs = 60 * 1000000; 

void setup() {
  Serial.begin(115200);
  Serial.println("ESP2 Starting...");

  // Initialize DHT sensor
  dht.begin();

  while (true) { 
    connectToWiFi();
    startServer();
    readLocalSensorData();
    waitForESP1Data();
    sendDataToRPi();
    enterLightSleep();
  }
}

void loop() {

}

void connectToWiFi() {
  // Initialize Wi-Fi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  // Wait for connection with timeout
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) { 
    delay(100);
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Connected to Wi-Fi");
  } else {
    Serial.println("Failed to connect to Wi-Fi, retrying in next cycle...");
  }
}

void startServer() {
  // Start server to receive data from ESP1
  server.on("/data", HTTP_POST, handleReceiveData);
  server.begin();
  Serial.println("Server started!");
}

void readLocalSensorData() {
  // Read local sensor data of ESP2
  temperatureESP2 = dht.readTemperature();
  humidityESP2 = dht.readHumidity();
}

void waitForESP1Data() {
  // Reset ESP1 data to ensure fresh data is expected in each cycle
  temperatureESP1 = NAN;
  humidityESP1 = NAN;

  Serial.println("Waiting for data from ESP1...");
  unsigned long timeout = millis() + 30000;
  while (isnan(temperatureESP1) && millis() < timeout) {
    server.handleClient();
  }

  // Check if data was received
  if (isnan(temperatureESP1)) {
    Serial.println("No data received from ESP1 within the timeout period.");
  } else {
    Serial.println("Data received from ESP1.");
  }
  server.stop();
}

void handleReceiveData() {
  if (server.hasArg("plain")) {
    String payload = server.arg("plain");
    Serial.println("Data received from ESP1: " + payload);

    // Parse the incoming JSON data
    DynamicJsonDocument doc(256);
    DeserializationError error = deserializeJson(doc, payload);

    if (!error) {
      temperatureESP1 = doc["temperature"];
      humidityESP1 = doc["humidity"];

      // Send acknowledgment to ESP1
      server.send(200, "text/plain", "Data received and processed!");
    } else {
      server.send(400, "text/plain", "Invalid JSON");
    }
  } else {
    server.send(400, "text/plain", "Invalid data");
  }
}

void sendDataToRPi() {
  // Use "N/A" if ESP1 data is not available
  String tempESP1 = isnan(temperatureESP1) ? "\"N/A\"" : String(temperatureESP1);
  String humESP1 = isnan(humidityESP1) ? "\"N/A\"" : String(humidityESP1);
  String tempESP2 = isnan(temperatureESP2) ? "\"N/A\"" : String(temperatureESP2);
  String humESP2 = isnan(humidityESP2) ? "\"N/A\"" : String(humidityESP2);

  // Prepare the JSON payload
  String payload = "{\"temperature_esp1\": " + tempESP1 +
                   ", \"humidity_esp1\": " + humESP1 +
                   ", \"temperature_esp2\": " + tempESP2 +
                   ", \"humidity_esp2\": " + humESP2 + "}";

  // Send data to Raspberry Pi
  if (WiFi.status() == WL_CONNECTED) {
    WiFiClient client;
    HTTPClient http;

    http.begin(client, rpiServerUrl);
    http.addHeader("Content-Type", "application/json");

    int httpCode = http.POST(payload);

    if (httpCode == HTTP_CODE_OK) {
      Serial.println("Data sent to Raspberry Pi!");
    } else {
      Serial.printf("Failed to send data: %s\n", http.errorToString(httpCode).c_str());
    }
    http.end();
  } else {
    Serial.println("Wi-Fi not connected. Cannot send data to Raspberry Pi.");
  }
}

void enterLightSleep() {
  // Configure Wi-Fi to disconnect and enter modem sleep
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);

  // Set sleep type to Light Sleep
  wifi_set_sleep_type(LIGHT_SLEEP_T);

  // Configure sleep duration
  wifi_fpm_set_sleep_type(LIGHT_SLEEP_T);
  wifi_fpm_open();
  wifi_fpm_do_sleep(sleepTimeUs);

  // The device will sleep here and resume after sleepTimeUs
  delay(sleepTimeUs / 1000 + 10); 

  // Wake up and reinitialize
  wifi_fpm_close();
  WiFi.mode(WIFI_STA);
  Serial.println("Woke up from Light Sleep");
}