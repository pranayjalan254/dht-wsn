#include <ESP8266WiFi.h>
#include <DHT.h>
#include <ESP8266HTTPClient.h>

// Wi-Fi configuration
const char* ssid = "Pranay";
const char* password = "shourya2010";

// Server URL
const char* serverUrl = "http://192.168.127.128:5000/data"; 

// DHT configuration
#define DHTPIN 2         // GPIO2
#define DHTTYPE DHT11    // DHT11
DHT dht(DHTPIN, DHTTYPE);

// Deep Sleep duration (in microseconds)
const uint64_t sleepTimeS = 60e6; 

void setup () {
    Serial.begin(115200);

    // Initialize DHT sensor
    dht.begin();

    // Optimize Wi-Fi connection
    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);

    // Connect to Wi-Fi
    WiFi.begin(ssid, password);

    // Wait for connection with timeout
    unsigned long startAttemptTime = millis();
    while (WiFi.status() != WL_CONNECTED) {
        delay(100);
        Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nConnected to Wi-Fi");

        // Read data from DHT sensor
        float temperature = dht.readTemperature();
        float humidity = dht.readHumidity();

        // Prepare JSON payload (use "N/A" if sensor data is unavailable)
        String tempStr = isnan(temperature) ? "\"N/A\"" : String(temperature);
        String humStr = isnan(humidity) ? "\"N/A\"" : String(humidity);

        Serial.print("Humidity: ");
        Serial.print(humStr);
        Serial.print(" %\t");
        Serial.print("Temperature: ");
        Serial.print(tempStr);
        Serial.println(" °C");

        String payload = "{\"temperature\": " + tempStr + ", \"humidity\": " + humStr + "}";

        // Send data to server
        WiFiClient client;
        HTTPClient http;

        http.begin(client, serverUrl);
        http.addHeader("Content-Type", "application/json");

        int httpCode = http.POST(payload);

        if (httpCode > 0) {
            Serial.printf("Data sent! Response code: %d\n", httpCode);
        } else {
            Serial.printf("Failed to send data: %s\n", http.errorToString(httpCode).c_str());
        }
        http.end();
    } else {
        Serial.println("\nFailed to connect to Wi-Fi");
    }

    Serial.println("Entering deep sleep for 1 minute");
    ESP.deepSleep(sleepTimeS);
}

void loop() {

}