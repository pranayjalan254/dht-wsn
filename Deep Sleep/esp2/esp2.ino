#include <ESP8266WiFi.h>
#include <DHT.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>

// Wi-Fi credentials
const char* ssid = "Pranay";
const char* password = "shourya2010";

// DHT configuration for local data aggregation
#define DHTPIN 2         // GPIO2
#define DHTTYPE DHT11    // DHT11
DHT dht(DHTPIN, DHTTYPE);

// Server for receiving data from ESP1
ESP8266WebServer server(5000);

// Variables to store aggregated data
float temperatureESP1 = NAN;
float humidityESP1 = NAN;
float temperatureESP2 = NAN;
float humidityESP2 = NAN;

// Raspberry Pi server URL
const char* rpiServerUrl = "http://192.168.24.129:2173/data";

// Deep Sleep duration (in microseconds)
const uint64_t sleepTimeS = 60e6; 

// Variables for timing
unsigned long startTime;
const unsigned long maxListeningTime = 5000; 

void setup() {
    Serial.begin(115200);

    // Initialize DHT sensor
    dht.begin();

    // Optimize Wi-Fi connection
    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);

    // Connect to Wi-Fi
    WiFi.begin(ssid, password);

    // Wait for connection with timeout
    unsigned long wifiStartTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - wifiStartTime < 5000) {
        delay(100);
        Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nConnected to Wi-Fi");

        // Start server to receive data from ESP1
        server.on("/data", HTTP_POST, handleReceiveData);
        server.begin();
        Serial.println("Server started!");
        Serial.print("IP Address: ");
        Serial.println(WiFi.localIP());

        // Read local sensor data of ESP2
        temperatureESP2 = dht.readTemperature();
        humidityESP2 = dht.readHumidity();

        // Start listening for data from ESP1
        startTime = millis();
        while (millis() - startTime < maxListeningTime) {
            server.handleClient();
            delay(10); // Small delay to prevent watchdog reset
        }

        // Send aggregated data to Raspberry Pi
        sendDataToRPi();

        // Stop server
        server.stop();
    } else {
        Serial.println("\nFailed to connect to Wi-Fi");
    }

    // Enter deep sleep
    Serial.println("Entering deep sleep for 5 minutes");
    ESP.deepSleep(sleepTimeS);
}

void loop() {
}

// Handle incoming data from ESP1
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

            // Send response to ESP1
            String response = "Data received and processed!";
            server.send(200, "text/plain", response);
        } else {
            Serial.println("Failed to parse JSON");
            server.send(400, "text/plain", "Invalid JSON");
        }
    } else {
        server.send(400, "text/plain", "Invalid data");
    }
}

// Send aggregated data to Raspberry Pi
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

        if (httpCode > 0) {
            Serial.printf("Data sent to Raspberry Pi! Response code: %d\n", httpCode);
        } else {
            Serial.printf("Failed to send data: %s\n", http.errorToString(httpCode).c_str());
        }
        http.end();
    } else {
        Serial.println("Wi-Fi not connected. Cannot send data to Raspberry Pi.");
    }
}
