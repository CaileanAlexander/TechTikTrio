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
   - Sends data to ThingSpeak with:
        lux, temperature, alarm, sensorState, sunrise/sunset, touchSeconds
   - Touch sensor logic with LED + timer
   - Sunrise/Sunset API integration
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
String urlPath = "https://script.google.com/macros/s/AKfycbyvCH5hwjrAa-l5IWyfQ9fRnb2gdTeuiJkp2mPZOD5weTPT8UAQtSgeiDZkbwMOFNSFUg/exec";

WiFiSSLClient wifi;
HttpClient client(wifi, server, port);

// ---------- ThingSpeak / Wi-Fi config ----------
const char THINGSPEAK_SERVER[] = "api.thingspeak.com";
const int THINGSPEAK_PORT = 80;
const float latitude  = 53.9;   // Sligo, Ireland
const float longitude = -8.5;
const String WRITE_API_KEY = "MKW5EOLZ4VMNMPAS";  // <-- PUT KEY HERE

WiFiClient wifiClient;
HttpClient httpClient(wifiClient, THINGSPEAK_SERVER, THINGSPEAK_PORT);

// Connect to sunrise-sunset.org API on port 80 (HTTP)
HttpClient sunClient(wifiClient, "api.sunrise-sunset.org", 80);

// ----------------- Alarm System Pins -----------------
#define LED 6
#define sensorPin 4
#define buzzerPin 8
#define buttonPin 3

// --- Touch sensor pins ---
const int touchsensorPin = D7;
const int touchLED = A3;

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
unsigned long googleSheetsMillis = 0;

// --- Touch sensor state ---
int lasttouchsensorState = LOW;        // Previous reading
int currenttouchsensorState;           // Current state
unsigned long touchStartTime = 0;      // When sensor was touched
bool touchTimerRunning = false;        // Track if timer is active
unsigned long lastTouchPrintMillis = 0; // Track last print for elapsed

// ----------------- Clock + Elapsed Time -----------------
unsigned long previousMillis = 0;
unsigned long lcdUpdateMillis = 0;
unsigned long printUpdateMillis = 0;
unsigned long thingspeakMillis = 0;
unsigned long displayCycleMillis = 0;
unsigned long sunMillis = 0;

const unsigned long lcdUpdateInterval = 1000;
const unsigned long printInterval = 5000;
const unsigned long thingspeakInterval = 20000UL;
const unsigned long displayCycleInterval = 3000UL;
const unsigned long sunInterval = 30000UL;

int seconds = 0;
int minutes = 0;
int hours = 0;

int elapsedSeconds = 0;
int elapsedMinutes = 0;
int elapsedHours = 0;

// sensor values
float tempC = 0.0;
float lightLux = 0.0;

// sunrise/sunset numeric values for ThingSpeak
float sunriseFloat = 0.0;
float sunsetFloat  = 0.0;

// display cycle state: 0=Running,1=Temp,2=Light
int displayState = 0;

// sunrise/sunset formatted times
String sunriseFormatted = "";
String sunsetFormatted  = "";

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
  const int BETA = 4275;       // beta coefficient
  const float R0 = 100000.0;   // 100kΩ at 25°C

  int a = analogRead(pin);
  if (a <= 0) return -100;

  float resistance = 1023.0 / a - 1.0;
  resistance = R0 * resistance;

  float temperatureKelvin = 1.0 / (log(resistance / R0) / BETA + 1.0 / 298.15);
  return temperatureKelvin - 273.15;
}

// ----------------- Grove Light Sensor -----------------
float readGroveLightLux(int pin) {
  int sensorValue = analogRead(pin);
  if (sensorValue <= 0) return 0;
  float resistance = (1023.0 - sensorValue) * 10.0 / sensorValue;
  return 10000.0 / pow(resistance, 4.0 / 3.0);
}

// ---------- ThingSpeak POST ----------
void postToThingSpeak(float lux, float temperature, bool alarm, int sensorState, unsigned long touchSeconds) {
  String body = "api_key=" + WRITE_API_KEY +
                "&field1=" + String(lux, 1) +
                "&field2=" + String(temperature, 1) +
                "&field3=" + String(alarm ? 1 : 0) +
                "&field4=" + String(sensorState) +
                "&field5=" + String(sunriseFloat, 2) +
                "&field6=" + String(sunsetFloat, 2) +
                "&field7=" + String(touchSeconds);

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

// ----------------- Sunrise/Sunset Helpers -----------------
String formatTime(String isoTime) {
  int tIndex = isoTime.indexOf('T');
  if (tIndex == -1) return "Invalid";

  String timePart = isoTime.substring(tIndex + 1, tIndex + 6);
  int hour = timePart.substring(0, 2).toInt();
  String minutes = timePart.substring(3, 5);

  String suffix = "AM";
  if (hour == 0) {
    hour;
    if (hour == 12) {
    suffix = "PM";
  } else if (hour > 12) {
    hour -= 12;
    suffix = "PM";
  }

  return Strisunsetng(hour) + ":" + minutes + " " + suffix;
  }
}

void getSunData() {
  String path = "/json?lat=" + String(latitude, 6) + "&lng=" + String(longitude, 6) + "&formatted=0";

  Serial.print("Requesting: ");
  Serial.println(path);

  sunClient.get(path);

  int statusCode = sunClient.responseStatusCode();
  String response = sunClient.responseBody();

  Serial.print("Status code: ");
  Serial.println(statusCode);
  Serial.println("---- JSON Response ----");
  Serial.println(response);
  Serial.println("------------------------");

  String sunrise = extractValue(response, "\"sunrise\":\"", "\"");
  String sunset  = extractValue(response, "\"sunset\":\"", "\"");

  sunriseFormatted = formatTime(sunrise);
  sunsetFormatted  = formatTime(sunset);

  sunriseFloat = timeToFloat(sunrise);
  sunsetFloat  = timeToFloat(sunset);

  Serial.print("Sunrise: ");
  Serial.println(sunriseFormatted);
  Serial.print("Sunset: ");
  Serial.println(sunsetFormatted);
  Serial.println("----------------------------------");

  sunClient.stop();
}

String extractValue(String data, String key, String delimiter) {
  int start = data.indexOf(key);
  if (start == -1) return "N/A";
  start += key.length();
  int end = data.indexOf(delimiter, start);
  if (end == -1) end = data.length();
  return data.substring(start, end);
}

String urlEncode(String s) {
  s.replace(" ", "%20");
  return s;
}

float timeToFloat(String isoTime) {
  int tIndex = isoTime.indexOf('T');
  if (tIndex == -1) return 0.0;

  String timePart = isoTime.substring(tIndex + 1, tIndex + 6);
  int hour = timePart.substring(0, 2).toInt();
  int minute = timePart.substring(3, 5).toInt();

  return hour + (minute / 100.0);
}

// ============================================================
//                         SETUP
// ============================================================
void setup() {
  Serial.begin(9600);
  delay(2000);
  Serial.println("Combined Smart Clock + ThingSpeak + Google Sheets");

  pinMode(buzzerPin, OUTPUT);
  pinMode(buttonPin, INPUT);
  pinMode(sensorPin, INPUT);
  pinMode(LED, OUTPUT);
  pinMode(touchsensorPin, INPUT);
  pinMode(touchLED, OUTPUT);
  digitalWrite(touchLED, LOW);

  lcd.begin(16, 2);
  lcd.setRGB(0, 100, 255);
  lcd.print("   TechTikTrio    ");
  lcd.setCursor(0, 1);
  lcd.print("   Atmosphere     ");
  delay(2000);
  lcd.clear();

  noTone(buzzerPin);
  buzzerActive = false;
  alarmTriggered = false;

  connectWiFi();

  previousMillis = millis();
  lcdUpdateMillis = previousMillis;
  thingspeakMillis = previousMillis;
  printUpdateMillis = previousMillis;
  displayCycleMillis = previousMillis;
  sunMillis = previousMillis;
}

// ============================================================
//                          LOOP
// ============================================================
void loop() {
  unsigned long currentMillis = millis();

  // --- clock update ---
  if (currentMillis - previousMillis >= 1000) {
    previousMillis += 1000;
    seconds++;
    elapsedSeconds++;

    if (seconds >= 60) { seconds = 0; minutes++; }
    if (minutes >= 60) { minutes = 0; hours++; }
    if (hours >= 24) hours = 0;

    if (elapsedSeconds >= 60) { elapsedSeconds = 0; elapsedMinutes++; }
    if (elapsedMinutes >= 60) { elapsedMinutes = 0; elapsedHours++; }
  }

  // --- read sensors ---
  tempC = readGroveTemperatureC(TEMP_PIN);
  lightLux = readGroveLightLux(LIGHT_PIN);
  int digitalSensorState = digitalRead(sensorPin);

  // --- sensor-triggered alarm ---
  if (digitalSensorState == HIGH && !buzzerActive) {
    buzzerActive = true;
    alarmTriggered = true;
    Serial.println("Sensor triggered buzzer - resetting clocks.");
    resetClocks();
  }

  // --- button debounce ---
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

  // --- Touch sensor timer / LED handling ---
  currenttouchsensorState = digitalRead(touchsensorPin);

  if (lasttouchsensorState == LOW && currenttouchsensorState == HIGH) {
    if (!touchTimerRunning) {
      Serial.println("Touch sensor activated - someone has entered.");
      touchStartTime = currentMillis;
      touchTimerRunning = true;
      digitalWrite(touchLED, HIGH);
    } else {
      Serial.println("Touch sensor activated - door closed.");
      unsigned long totalTime = currentMillis - touchStartTime;
      Serial.print("Door was open for: ");
      Serial.print(totalTime / 1000);
      Serial.println(" seconds.");
      touchTimerRunning = false;
      touchStartTime = 0;
      digitalWrite(touchLED, LOW);
    }
  }

  if (touchTimerRunning && currentMillis - lastTouchPrintMillis >= 1000) {
    lastTouchPrintMillis = currentMillis;
    unsigned long elapsed = currentMillis - touchStartTime;
    Serial.print("Seconds since someone has entered: ");
    Serial.println(elapsed / 1000);
  }

  lasttouchsensorState = currenttouchsensorState;

  // --- LCD top row time ---
  if (currentMillis - lcdUpdateMillis >= lcdUpdateInterval) {
    lcdUpdateMillis += lcdUpdateInterval;

    lcd.setCursor(0, 0);
    lcd.print("Time:");
    lcd.setCursor(6, 0);
    if (hours < 10) lcd.print('0'); lcd.print(hours);
    lcd.print(':');
    if (minutes < 10) lcd.print('0'); lcd.print(minutes);
    lcd.print(':');
    if (seconds < 10) lcd.print('0'); lcd.print(seconds);
  }

  // --- LCD bottom row ---
  if (buzzerActive) {
    lcd.setCursor(0, 1);
    if ((millis() / 500) % 2 == 0) {
      lcd.print("    WARNING!     ");
      lcd.setRGB(255, 0, 0);
    } else {
      lcd.print("                 ");
      lcd.setRGB(100, 0, 0);
    }
  } else {
    if (millis() - displayCycleMillis >= displayCycleInterval) {
      displayCycleMillis += displayCycleInterval;
      displayState = (displayState + 1) % 3;
    }

    lcd.setCursor(0, 1);

    if (displayState == 0) {
      lcd.print("Running...       ");
      lcd.setRGB(0, 100, 255);
    } else if (displayState == 1) {
      char buf[17];
      snprintf(buf, sizeof(buf), "Temp:%5.1f C      ", tempC);
      lcd.print(buf);
      lcd.setRGB(0, 150, 200);

      // touch timer display (right-aligned)
      if (touchTimerRunning) {
        char touchBuf[10];
        unsigned long elapsed = (millis() - touchStartTime) / 1000;
        snprintf(touchBuf, sizeof(touchBuf), "T:%2lus", elapsed);
        lcd.setCursor(16 - strlen(touchBuf), 1);
        lcd.print(touchBuf);
      }

    } else { // displayState == 2
      char buf[17];
      snprintf(buf, sizeof(buf), "Light:%6.0f lx    ", lightLux);
      lcd.print(buf);
      lcd.setRGB(120, 100, 0);

      // touch timer display (right-aligned)
      if (touchTimerRunning) {
        char touchBuf[10];
        unsigned long elapsed = (millis() - touchStartTime) / 1000;
        snprintf(touchBuf, sizeof(touchBuf), "T:%2lus", elapsed);
        lcd.setCursor(16 - strlen(touchBuf), 1);
        lcd.print(touchBuf);
      }
    }
  }

  // --- buzzer + LED ---
  if (buzzerActive) tone(buzzerPin, 1000);
  else noTone(buzzerPin);

  digitalWrite(LED, buzzerActive ? HIGH : LOW);

  // --- Sunrise/Sunset update ---
  if (currentMillis - sunMillis >= sunInterval) {
    sunMillis = currentMillis;
    getSunData();
  }

  // --- ThingSpeak upload ---
  if (currentMillis - thingspeakMillis >= thingspeakInterval) {
    thingspeakMillis += thingspeakInterval;

    unsigned long touchSeconds = touchTimerRunning ? (millis() - touchStartTime) / 1000 : 0;
    postToThingSpeak(lightLux, tempC, buzzerActive, digitalRead(sensorPin), touchSeconds);
  }

  // --- Google Sheets upload every 5 sec ---
  if (currentMillis - googleSheetsMillis >= printInterval) {
    googleSheetsMillis += printInterval;
    postToGoogleSheets(lightLux, tempC, alarmTriggered, elapsedSeconds);
  }
}

