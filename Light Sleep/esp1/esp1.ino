#include <ESP8266WiFi.h>
#include <DHT.h>
#include <ESP8266HTTPClient.h>
extern "C" {
  #include "user_interface.h"
}

// Wi-Fi configuration
const char* ssid = "Pranay";
const char* password = "shourya2010";

// Server URL
const char* serverUrl = "http://192.168.127.73:5000/data"; 

// DHT configuration
#define DHTPIN 2         // GPIO2
#define DHTTYPE DHT11    // DHT11 Sensor
DHT dht(DHTPIN, DHTTYPE);

// Sleep duration in microseconds (1 minute)
const uint32_t sleepTimeUs = 60 * 1000000; 



void setup() {
  Serial.begin(115200);
  Serial.println("ESP1 Starting...");

  // Initialize DHT sensor
  dht.begin();

  // Initialize Wi-Fi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Connected to Wi-Fi");

    // Read data from DHT sensor
    float temperature = dht.readTemperature();
    float humidity = dht.readHumidity();

    // Prepare JSON payload
    String tempStr = isnan(temperature) ? "\"N/A\"" : String(temperature);
    String humStr = isnan(humidity) ? "\"N/A\"" : String(humidity);

    String payload = "{\"temperature\": " + tempStr + ", \"humidity\": " + humStr + "}";

    bool acknowledged = false;

    while (!acknowledged) {
      acknowledged = sendDataToESP2(payload);

      if (!acknowledged) {
        Serial.println("Acknowledgment not received. Retrying...");

        delay(2000);
      }
    }

    if (acknowledged) {
      Serial.println("Acknowledgment received. Data sent successfully!");
    } else {
      Serial.println("Failed to get acknowledgment after max retries.");
    }
  } else {
    Serial.println("Failed to connect to Wi-Fi");
  }

  // Enter Light Sleep
  Serial.println("Entering Light Sleep for 1 minute...");
  enterLightSleep();
}

void loop() {
}

bool sendDataToESP2(const String& payload) {
  WiFiClient client;
  HTTPClient http;

  http.begin(client, serverUrl);
  http.addHeader("Content-Type", "application/json");

  int httpCode = http.POST(payload);

  // Check if the response code is 200 OK (Acknowledgment received)
  if (httpCode == HTTP_CODE_OK) {
    Serial.println("Data sent to ESP2 successfully!");
    http.end();
    return true;
  } else {
    Serial.printf("Failed to send data to ESP2: %s\n", http.errorToString(httpCode).c_str());
    http.end();
    return false;
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
  delay(sleepTimeUs / 1000 + 10); // Wait for the sleep duration plus a small buffer

  // Wake up and reinitialize
  wifi_fpm_close();
  WiFi.mode(WIFI_STA);
  Serial.println("Woke up from Light Sleep");
  setup(); 
}