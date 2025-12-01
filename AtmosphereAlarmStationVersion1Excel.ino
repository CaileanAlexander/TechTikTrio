/*
   Combined System:
   - Grove Light Sensor (A0)
   - Grove Temperature Sensor (A1)
   - LCD Alarm System
   - Sends JSON to Google Sheets with:
        temperature
        light
        alarmTriggered
        elapsedSeconds
*/

// ----------------- Libraries -----------------
#include <WiFiS3.h>
#include <WiFiSSLClient.h>
#include <ArduinoHttpClient.h>
#include "arduino_secrets.h"

#include <Wire.h>
#include "rgb_lcd.h"

// ----------------- Wi-Fi -----------------
char ssid[] = SECRET_SSID;
char pass[] = SECRET_PASS;

// Google Script Web App URL (EXEC)
const char server[] = "script.google.com";
const int port = 443; // HTTPS
String urlPath = "https://script.google.com/macros/s/AKfycbxS960HAXuXoG6uq_R381VK0-TxhlpfXju5_OICyVdcM00q7LNwQO0acWaii1YXuZMRfg/exec";

WiFiSSLClient wifi;
HttpClient client(wifi, server, port);

// ----------------- Alarm System Pins -----------------
#define LED 6
#define sensorPin 4
#define buzzerPin 8
#define buttonPin 3

// ----------------- Sensors -----------------
#define LIGHT_PIN A0
#define TEMP_PIN A1

rgb_lcd lcd;

// ----------------- Alarm System Variables -----------------
bool buzzerActive = false;
bool alarmTriggered = false;

int lastButtonState = LOW;
int buttonState = LOW;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;

// ----------------- Clock + Elapsed Time -----------------
unsigned long previousMillis = 0;
unsigned long lcdUpdateMillis = 0;
unsigned long printUpdateMillis = 0;
const unsigned long lcdUpdateInterval = 1000;
const unsigned long printInterval = 5000;

int seconds = 0;
int minutes = 0;
int hours = 0;

int elapsedSeconds = 0;
int elapsedMinutes = 0;
int elapsedHours = 0;

// ----------------- Wi-Fi -----------------
void connectWiFi() {
  Serial.print("Connecting to Wi-Fi");
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}

// ----------------- Grove Temperature Sensor -----------------
float readGroveTemperatureC(int pin) {
  int sensorValue = analogRead(pin);
  if (sensorValue == 0) return NAN;
  float resistance = (1023.0 - sensorValue) * 10000.0 / sensorValue;
  const float B = 3975.0;
  float temperatureK = 1.0 / ((1.0/298.15) + (1.0/B) * log(resistance / 10000.0));
  return temperatureK - 273.15;
}

// ----------------- Grove Light Sensor -----------------
float readGroveLightLux(int pin) {
  int sensorValue = analogRead(pin);
  if (sensorValue == 0) return 0;
  float resistance = (float)(1023 - sensorValue) * 10.0 / sensorValue;
  return 10000.0 / pow(resistance, 4.0/3.0);
}

// ----------------- POST to Google Sheets -----------------
void postToGoogleSheets(float light, float temperature, bool alarmTriggered, int elapsedSeconds) {

  String json = "{";
  json += "\"temperature\":" + String(temperature, 2) + ",";
  json += "\"light\":" + String(light, 2) + ",";
  json += "\"alarmTriggered\":" + String(alarmTriggered ? "true" : "false") + ",";
  json += "\"elapsedSeconds\":" + String(elapsedSeconds);
  json += "}";

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

// ----------------- Reset Clock -----------------
void resetClock() {
  seconds = minutes = hours = 0;
  elapsedSeconds = elapsedMinutes = elapsedHours = 0;
}

// ============================================================
//                         SETUP
// ============================================================
void setup() {
  Serial.begin(9600);
  delay(1500);

  connectWiFi();

  pinMode(buzzerPin, OUTPUT);
  pinMode(buttonPin, INPUT);
  pinMode(sensorPin, INPUT);
  pinMode(LED, OUTPUT);

  lcd.begin(16, 2);
  lcd.setRGB(0, 100, 255);
  lcd.print("  TechTikTrio");
  lcd.setCursor(0, 1);
  lcd.print("  Atmosphere Alarm");
  delay(2000);
  lcd.clear();
}

// ============================================================
//                          LOOP
// ============================================================
void loop() {

  unsigned long currentMillis = millis();

  // ---------------- Clock ----------------
  if (currentMillis - previousMillis >= 1000) {
    previousMillis = currentMillis;
    seconds++;
    elapsedSeconds++;

    if (seconds >= 60) { seconds = 0; minutes++; }
    if (minutes >= 60) { minutes = 0; hours++; }
    if (hours >= 24) { hours = 0; }

    if (elapsedSeconds >= 60) { elapsedSeconds = 0; elapsedMinutes++; }
    if (elapsedMinutes >= 60) { elapsedMinutes = 0; elapsedHours++; }
  }

  // ---------------- Read Sensors ----------------
  float temperature = readGroveTemperatureC(TEMP_PIN);
  float light = readGroveLightLux(LIGHT_PIN);
  int sensorState = digitalRead(sensorPin);

  // ---------------- Button Debounce ----------------
  int reading = digitalRead(buttonPin);
  if (reading != lastButtonState) {
    lastDebounceTime = currentMillis;
  }
  if ((currentMillis - lastDebounceTime) > debounceDelay) {
    if (reading != buttonState) {
      buttonState = reading;
      if (buttonState == HIGH) {
        buzzerActive = !buzzerActive;
        if (buzzerActive) {
          alarmTriggered = true;
          resetClock();
        } else {
          alarmTriggered = false;
        }
      }
    }
  }
  lastButtonState = reading;

  // ---------------- Sensor Trigger ----------------
  if (sensorState == HIGH && !buzzerActive) {
    buzzerActive = true;
    alarmTriggered = true;
    resetClock();
  }

  // ---------------- LCD Update ----------------
  if (currentMillis - lcdUpdateMillis >= lcdUpdateInterval) {
    lcdUpdateMillis = currentMillis;

    lcd.setCursor(0, 0);
    lcd.print("T:");
    lcd.print(hours < 10 ? "0" : "");
    lcd.print(hours);
    lcd.print(":");
    lcd.print(minutes < 10 ? "0" : "");
    lcd.print(minutes);
    lcd.print(":");
    lcd.print(seconds < 10 ? "0" : "");
    lcd.print(seconds);

    lcd.setCursor(0, 1);
    if (buzzerActive) {
      lcd.print("   WARNING!    ");
      lcd.setRGB(255, 0, 0);
    } else {
      lcd.print(" Temp:");
      lcd.print(temperature, 1);
      lcd.print("C  ");
      lcd.setRGB(0, 100, 255);
    }
  }

  // ---------------- Buzzer + LED ----------------
  if (buzzerActive) tone(buzzerPin, 1000);
  else noTone(buzzerPin);

  digitalWrite(LED, buzzerActive ? HIGH : LOW);

  // ---------------- Serial + POST every 5 sec ----------------
  if (currentMillis - printUpdateMillis >= printInterval) {
    printUpdateMillis = currentMillis;

    Serial.print("Temp: "); Serial.print(temperature);
    Serial.print(" | Light: "); Serial.print(light);
    Serial.print(" | Alarm: "); Serial.print(alarmTriggered);
    Serial.print(" | ElapsedSec: "); Serial.println(elapsedSeconds);

    postToGoogleSheets(light, temperature, alarmTriggered, elapsedSeconds);
  }
}
