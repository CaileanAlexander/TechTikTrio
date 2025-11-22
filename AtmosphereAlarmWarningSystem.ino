/*
  Combined Smart Clock + Sensors + ThingSpeak POST
  - Temp sensor (LM35): A0
  - Light sensor: A1
  - Buzzer: D8
  - Button: D3 (debounced)
  - Sensor trigger (digital): D4
  - LED: D6
  - RGB LCD: I2C (rgb_lcd)
  - ThingSpeak POST every 20 seconds
*/

#include <Wire.h>
#include "rgb_lcd.h"
#include <WiFiS3.h>
#include <ArduinoHttpClient.h>
#include "arduino_secrets.h"   // define SECRET_SSID and SECRET_PASS

// ---------- ThingSpeak / Wi-Fi config ----------
const char THINGSPEAK_SERVER[] = "api.thingspeak.com";
const int THINGSPEAK_PORT = 80;
// Replace with your channel Write API Key
const String WRITE_API_KEY = "YTET0VR0IH9MB2U3";

WiFiClient wifiClient;
HttpClient httpClient(wifiClient, THINGSPEAK_SERVER, THINGSPEAK_PORT);

// ---------- Pins ----------
#define LED_PIN 6
#define TEMP_PIN A0     // LM35 temperature sensor
#define LIGHT_PIN A1    // Grove light sensor
#define SENSOR_PIN 4    // External digital sensor trigger (D4)
const int BUZZER_PIN = 8;
const int BUTTON_PIN = 3;

// ---------- LCD ----------
rgb_lcd lcd;

// ---------- State ----------
bool buzzerActive = false;
bool alarmTriggered = false;

int lastButtonState = LOW;
int buttonState = LOW;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50; // ms

// clock timing
unsigned long previousMillis = 0;         // increments each second
unsigned long lcdUpdateMillis = 0;        // for top-row time updates (1s)
unsigned long thingspeakMillis = 0;       // ThingSpeak upload interval
unsigned long printUpdateMillis = 0;      // Serial print interval
unsigned long displayCycleMillis = 0;     // bottom-row cycle timer

const unsigned long secondInterval = 1000UL;
const unsigned long lcdInterval = 1000UL;
const unsigned long thingspeakInterval = 20000UL; // 20 seconds
const unsigned long printInterval = 5000UL;       // 5 seconds
const unsigned long displayCycleInterval = 3000UL; // 3 seconds for cycling messages

int seconds = 0;
int minutes = 0;
int hours = 0;

// elapsed since last reset
int elapsedSeconds = 0;
int elapsedMinutes = 0;
int elapsedHours = 0;

// sensor values
float tempC = 0.0;
float lightLux = 0.0;

// display cycle state: 0=Running,1=Temp,2=Light
int displayState = 0;

void connectWiFi() {
  Serial.print("Connecting to Wi-Fi");
  WiFi.begin(SECRET_SSID, SECRET_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to Wi-Fi!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}

// ---------- Sensor helper functions ----------
float readLm35C(int pin) {
  int raw = analogRead(pin);
  // LM35: 10 mV/°C ; ADC step ~4.887585 mV -> °C = raw * 0.48828125
  return raw * 0.48828125f;
}

float readGroveLightLux(int pin) {
  int sensorValue = analogRead(pin);
  if (sensorValue <= 0) return 0;
  // Convert ADC reading to resistance with 10k reference resistor (approx)
  float resistance = (1023.0 - sensorValue) * 10.0 / sensorValue; // in kΩ
  float lux = 10000.0 / pow(resistance, 4.0 / 3.0); // empirical approx
  return lux;
}

// ---------- ThingSpeak POST ----------
void postToThingSpeak(float lux, float temperature, bool alarm, int sensorState) {
  String body = "api_key=" + WRITE_API_KEY +
                "&field1=" + String(lux, 1) +
                "&field2=" + String(temperature, 1) +
                "&field3=" + String(alarm ? 1 : 0) +
                "&field4=" + String(sensorState);
  Serial.println("\nPosting to ThingSpeak:");
  Serial.println(body);

  httpClient.beginRequest();
  httpClient.post("/update");
  httpClient.sendHeader("Content-Type", "application/x-www-form-urlencoded");
  httpClient.sendHeader("Content-Length", body.length());
  httpClient.beginBody();
  httpClient.print(body);
  httpClient.endRequest();

  int status = httpClient.responseStatusCode();
  String resp = httpClient.responseBody();
  Serial.print("ThingSpeak status: ");
  Serial.println(status);
  Serial.print("Response: ");
  Serial.println(resp);

  httpClient.stop();
}

// ---------- Utility prints ----------
void printTimeSerial(int h, int m, int s) {
  if (h < 10) Serial.print('0');
  Serial.print(h);
  Serial.print(':');
  if (m < 10) Serial.print('0');
  Serial.print(m);
  Serial.print(':');
  if (s < 10) Serial.print('0');
  Serial.print(s);
}

void resetClocks() {
  seconds = 0;
  minutes = 0;
  hours = 0;
  elapsedSeconds = 0;
  elapsedMinutes = 0;
  elapsedHours = 0;
}

void setup() {
  Serial.begin(9600);
  delay(2000);
  Serial.println("Combined Smart Clock + ThingSpeak (Light + Temp)");

  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT);    // assumes external pull-down; change to INPUT_PULLUP if using internal pull-up
  pinMode(SENSOR_PIN, INPUT);    // digital sensor trigger
  pinMode(LED_PIN, OUTPUT);

  lcd.begin(16, 2);
  lcd.setRGB(0, 100, 255);
  lcd.print("   TechTikTrio    ");
  lcd.setCursor(0, 1);
  lcd.print("   Atmosphere     ");
  delay(2000);
  lcd.clear();

  noTone(BUZZER_PIN);
  buzzerActive = false;
  alarmTriggered = false;

  connectWiFi();

  // initialize timers
  previousMillis = millis();
  lcdUpdateMillis = previousMillis;
  thingspeakMillis = previousMillis;
  printUpdateMillis = previousMillis;
  displayCycleMillis = previousMillis;
}

void loop() {
  unsigned long currentMillis = millis();

  // --- clock update every 1s ---
  if (currentMillis - previousMillis >= secondInterval) {
    previousMillis += secondInterval;
    seconds++;
    elapsedSeconds++;

    if (seconds >= 60) { seconds = 0; minutes++; }
    if (minutes >= 60) { minutes = 0; hours++; }
    if (hours >= 24) hours = 0;

    if (elapsedSeconds >= 60) { elapsedSeconds = 0; elapsedMinutes++; }
    if (elapsedMinutes >= 60) { elapsedMinutes = 0; elapsedHours++; }
  }

  // --- read sensors (fast, non-blocking) ---
  tempC = readLm35C(TEMP_PIN);
  lightLux = readGroveLightLux(LIGHT_PIN);
  int digitalSensorState = digitalRead(SENSOR_PIN);

  // --- sensor-triggered alarm (external digital sensor) ---
  if (digitalSensorState == HIGH && !buzzerActive) {
    buzzerActive = true;
    alarmTriggered = true;
    Serial.println("Sensor triggered buzzer - resetting clocks.");
    resetClocks();
  }

  // --- button debounce & handling ---
  int reading = digitalRead(BUTTON_PIN);
  if (reading != lastButtonState) {
    lastDebounceTime = currentMillis;
  }
  if ((currentMillis - lastDebounceTime) > debounceDelay) {
    if (reading != buttonState) {
      buttonState = reading;
      // assuming button reads HIGH when pressed (external pull-down)
      if (buttonState == HIGH) {
        buzzerActive = !buzzerActive;
        if (buzzerActive) {
          Serial.println("Button activated buzzer - resetting clocks.");
          resetClocks();
        } else {
          Serial.println("Button deactivated buzzer.");
          alarmTriggered = false;
        }
      }
    }
  }
  lastButtonState = reading;

  // --- LCD top row updates every second (time) and bottom row cycles every 3s when no alarm ---
  if (currentMillis - lcdUpdateMillis >= lcdInterval) {
    lcdUpdateMillis += lcdInterval;

    // Top row: time
    lcd.setCursor(0, 0);
    lcd.print("Time:");
    lcd.setCursor(6, 0);
    if (hours < 10) lcd.print('0');
    lcd.print(hours);
    lcd.print(':');
    if (minutes < 10) lcd.print('0');
    lcd.print(minutes);
    lcd.print(':');
    if (seconds < 10) lcd.print('0');
    lcd.print(seconds);
  }

  // bottom row: choose behavior (alarm overrides)
  if (buzzerActive) {
    // blink warning every 500ms
    lcd.setCursor(0, 1);
    if ((currentMillis / 500) % 2 == 0) {
      lcd.print("    WARNING!     ");
      lcd.setRGB(255, 0, 0);
    } else {
      lcd.print("                 ");
      lcd.setRGB(100, 0, 0);
    }
  } else {
    // cycle bottom row every displayCycleInterval
    if (currentMillis - displayCycleMillis >= displayCycleInterval) {
      displayCycleMillis += displayCycleInterval;
      displayState = (displayState + 1) % 3; // 0,1,2
    }

    lcd.setCursor(0, 1);
    if (displayState == 0) {
      lcd.print("Running...       ");
      lcd.setRGB(0, 100, 255);
    } else if (displayState == 1) {
      // show temperature
      char buf[17];
      // format like "Temp: 23.4 C   "
      snprintf(buf, sizeof(buf), "Temp:%5.1f C      ", tempC);
      lcd.print(buf);
      lcd.setRGB(0, 150, 200);
    } else {
      // show light
      char buf[17];
      // format like "Light: 123 lx    "
      snprintf(buf, sizeof(buf), "Light:%6.0f lx    ", lightLux);
      lcd.print(buf);
      lcd.setRGB(120, 100, 0);
    }
  }

  // --- buzzer and LED control ---
  if (buzzerActive) tone(BUZZER_PIN, 1000);
  else noTone(BUZZER_PIN);

  digitalWrite(LED_PIN, buzzerActive ? HIGH : LOW);

  // --- ThingSpeak upload every thingspeakInterval ---
  if (currentMillis - thingspeakMillis >= thingspeakInterval) {
    thingspeakMillis += thingspeakInterval;
    postToThingSpeak(lightLux, tempC, buzzerActive, digitalRead(SENSOR_PIN));
  }

  // --- Serial debug every printInterval ---
  if (currentMillis - printUpdateMillis >= printInterval) {
    printUpdateMillis += printInterval;
    Serial.print("Clock: ");
    printTimeSerial(hours, minutes, seconds);
    Serial.print(" | Elapsed: ");
    printTimeSerial(elapsedHours, elapsedMinutes, elapsedSeconds);
    Serial.print(" | Buzzer: ");
    Serial.print(buzzerActive ? "ON" : "OFF");
    Serial.print(" | Sensor: ");
    Serial.print(digitalRead(SENSOR_PIN));
    Serial.print(" | Lux: ");
    Serial.print(lightLux, 1);
    Serial.print(" | Temp: ");
    Serial.print(tempC, 1);
    Serial.println(" C");
  }

  // loop remains non-blocking
}

