/*
  IoT Example: Uploading Grove Sensor Data to Google Sheets (POST)
  -------------------------------------------------------------
  Hardware:
    • Arduino UNO R4 WiFi
    • Grove Base Hat
    • Grove Light Sensor → A0
    • Grove Temperature Sensor → A1

  Description:
    Reads light and temperature values and sends them
    to a Google Sheets Web App as JSON via HTTPS POST.

  Focus:
    • HTTPS POST (WiFiSSLClient + HttpClient)
    • JSON body
*/

#include <WiFiS3.h>            // Wi-Fi for UNO R4 WiFi
#include <WiFiSSLClient.h>     // HTTPS support
#include <ArduinoHttpClient.h> // For HTTP requests
#include "arduino_secrets.h"   // Wi-Fi credentials

// ---- Wi-Fi credentials ----
// arduino_secrets.h should contain:
// #define SECRET_SSID "YourWiFiName"
// #define SECRET_PASS "YourWiFiPassword"
char ssid[] = SECRET_SSID;
char pass[] = SECRET_PASS;

// ---- Google Web App endpoint ----
const char server[] = "script.google.com";
const int port = 443;  // HTTPS
String urlPath = "https://script.google.com/macros/s/AKfycbwafLRD52lfqaQxjpDj93Pwu10x36pWrUyuC9Uzl3WHJ5qMlWpYzjzJCp7qYQiQydsqXw/exec";  // <-- Replace with your URL

WiFiSSLClient wifi;
HttpClient client(wifi, server, port);

// -----------------------------------------------------------------------------
// Connect to Wi-Fi
// -----------------------------------------------------------------------------
void connectWiFi() {
  Serial.print("Connecting to Wi-Fi");
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
}

// -----------------------------------------------------------------------------
// Read temperature (°C) from Grove analog sensor (NTC thermistor)
// -----------------------------------------------------------------------------
float readGroveTemperatureC(int pin) {
  int sensorValue = analogRead(pin);
  if (sensorValue == 0) return NAN;

  float resistance = (1023.0 - sensorValue) * 10000.0 / sensorValue;
  const float B = 3975.0;
  const float R0 = 10000.0;
  const float T0 = 298.15;
  float temperatureK = 1.0 / ((1.0 / T0) + (1.0 / B) * log(resistance / R0));
  return temperatureK - 273.15;
}

// -----------------------------------------------------------------------------
// Read light level (lux) from Grove analog light sensor
// -----------------------------------------------------------------------------
float readGroveLightLux(int pin) {
  int sensorValue = analogRead(pin);
  if (sensorValue == 0) return 0;
  float resistance = (float)(1023 - sensorValue) * 10.0 / sensorValue;
  float lux = 10000.0 / pow(resistance, 4.0 / 3.0);
  return lux;
}

// -----------------------------------------------------------------------------
// POST to Google Sheets
// -----------------------------------------------------------------------------
void postToGoogleSheets(float lux, float temperature) {
  // Build JSON body
  String json = "{\"light\":" + String(lux) + ",\"temperature\":" + String(temperature) + "}";

  Serial.println("\nPosting to Google Sheets...");
  Serial.println(json);

  client.beginRequest();
  client.post(urlPath);
  client.sendHeader("Content-Type", "application/json");
  client.sendHeader("Content-Length", json.length());
  client.beginBody();
  client.print(json);
  client.endRequest();

  int statusCode = client.responseStatusCode();
  String response = client.responseBody();

  Serial.print("Status code: ");
  Serial.println(statusCode);
  Serial.print("Response: ");
  Serial.println(response);
  Serial.println("-----------------------------");

  client.stop();
}

// -----------------------------------------------------------------------------
// Arduino Setup
// -----------------------------------------------------------------------------
void setup() {
  Serial.begin(9600);
  delay(2000);
  Serial.println("=== Google Sheets POST: Grove Sensors ===");
  connectWiFi();
}

// -----------------------------------------------------------------------------
// Arduino Loop
// -----------------------------------------------------------------------------
void loop() {
  float temperature = readGroveTemperatureC(A1);
  float lux = readGroveLightLux(A0);

  Serial.print("Light: ");
  Serial.print(lux);
  Serial.print(" lx  |  Temp: ");
  Serial.print(temperature);
  Serial.println(" °C");

  postToGoogleSheets(lux, temperature);
  delay(10000); // 10 s between updates
}
