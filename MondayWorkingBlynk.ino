/*************************************************************
  Combined: Atmosphere Warning System + Smart Clock + ThingSpeak
  Board: Arduino UNO R4 WiFi
  Blynk Console ready (virtual pins mapped)
*************************************************************/

#define BLYNK_PRINT Serial

// Blynk Template/Auth - keep as provided or replace with your token
#define BLYNK_TEMPLATE_ID "TMPL4IUVgIjiU"
#define BLYNK_TEMPLATE_NAME "Atmosphere Warning System"
#define BLYNK_AUTH_TOKEN "l0dQ7-rPioHBZFnS-T7ks_9liH_BOFJ"

// --------- LIBRARIES ---------
#include <SPI.h>
#include <Wire.h>
#include "rgb_lcd.h"
#include <WiFiS3.h>
#include <BlynkSimpleWifi.h>
#include <ArduinoHttpClient.h>
#include "arduino_secrets.h"   // define SECRET_SSID and SECRET_PASS

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
// ---------- Pins ----------
#define LED_PIN 6
#define TEMP_PIN A0     // LM35 temperature sensor
#define LIGHT_PIN A1    // Grove light sensor
#define TOUCH_PIN 4     // sensor trigger or touch (digital)
const int BUZZER_PIN = 8;
const int BUTTON_PIN = 3;

// ---------- Constants ----------
const float TEMP_MIN = 10.0f;  // degrees Celsius
const float TEMP_MAX = 27.0f;  // degrees Celsius

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
unsigned long lcdUpdateMillis = 0;        // top-row time updates (1s)
unsigned long thingspeakMillis = 0;       // ThingSpeak upload interval
unsigned long printUpdateMillis = 0;      // Serial print interval
unsigned long displayCycleMillis = 0;     // bottom-row cycle timer

const unsigned long secondInterval = 1000UL;
const unsigned long lcdInterval = 1000UL;
const unsigned long thingspeakInterval = 20000UL; // 20 seconds
const unsigned long printInterval = 5000UL;       // 5 seconds
const unsigned long displayCycleInterval = 3000UL; // 3 seconds

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

// sunrise/sunset numeric values for ThingSpeak
float sunriseFloat = 0.0;
float sunsetFloat  = 0.0;

unsigned long sunMillis = 0;
const unsigned long sunInterval = 30000UL; // 30 seconds

// display cycle state: 0=Running,1=Temp,2=Light
int displayState = 0;

// sunrise/sunset formatted times
String sunriseFormatted = "";
String sunsetFormatted  = "";

// Define alarm causes
enum AlarmCause {
  NONE = 0,
  TEMP_LOW,
  TEMP_HIGH,
  SENSOR_TRIGGER
};

AlarmCause currentAlarmCause = NONE;

// ---------- Blynk Virtual pin mapping (used in comments)
// V0  = Temperature (float)
// V1  = Light (float)
// V2  = Alarm State (bool)
// V3  = Button Control (write -> toggle/silence)
// V4  = External API Message (not used here)
// V5  = External Sensor State (digital)
// V6  = Time Elapsed (string "HH:MM:SS")
// V7  = Clock Time (string "HH:MM:SS")
// V8  = Manual Time Input ("HH:MM")
// V9  = Manual Buzzer Control (0/1)
// V10 = Manual LED Control (0/1)
// V11 = LCD Display Text (string)

// ---------- Sensor helper functions ----------
// Replace this existing function:
// float readLm35C(int pin) {
//   int raw = analogRead(pin);        // 12-bit ADC: 0–4095
//   float voltage = (raw * 3.3) / 4095.0;  // ✅ UNO R4 uses 3.3V ADC reference
//   float tempC = voltage * 100.0;         // ✅ LM35: 10mV per °C
//   return tempC;
// }

// WITH this Grove NTC thermistor reading function:
float readGroveTempC(int pin) {
  const int BETA = 4275;       // beta coefficient for the thermistor
  const float R0 = 100000.0;   // 100kΩ at 25°C
  
  int raw = analogRead(pin);
  if (raw <= 0) return -100.0; // error fallback
  
  // Convert ADC value (0-4095) to resistance of thermistor
  // Using 12-bit ADC and voltage divider formula
  float resistance = (4095.0 / (float)raw - 1.0) * R0;
  
  // Calculate temperature in Kelvin using Beta formula
  float tempK = 1.0 / (log(resistance / R0) / BETA + 1.0 / 298.15);
  
  // Convert Kelvin to Celsius
  float tempC = tempK - 273.15;
  return tempC;
}




float readGroveLightLux(int pin) {
  int sensorValue = analogRead(pin);
  if (sensorValue <= 0) return 0;
  // Convert ADC reading to resistance with 10k reference resistor (approx)
  float resistance = (4095.0 - sensorValue) * 10.0 / sensorValue; // in kΩ
  float lux = 10000.0 / pow(resistance, 4.0 / 3.0); // empirical approx
  return lux;
}

// ------- Formats display of the sunrise at sunset at the specified location --------
 
// Formats ISO 8601 time (e.g., 2025-12-01T08:27:15+00:00)
// into 12-hour format (e.g., 8:27 AM)
String formatTime(String isoTime) {
  int tIndex = isoTime.indexOf('T');
  if (tIndex == -1) return "Invalid";
 
  String timePart = isoTime.substring(tIndex + 1, tIndex + 6); // "08:27"
  int hour = timePart.substring(0, 2).toInt();
  String minutes = timePart.substring(3, 5);
 
  String suffix = "AM";
  if (hour == 0) {
    hour = 12;
  } else if (hour == 12) {
    suffix = "PM";
  } else if (hour > 12) {
    hour -= 12;
    suffix = "PM";
  }
 
  return String(hour) + ":" + minutes + " " + suffix;
}
 
 
// ----------------------------------------------------------
// Sends a request to Sunrise-Sunset API and prints result
// ----------------------------------------------------------
void getSunData() {
  // Build the path for the request
  String path = "/json?lat=" + String(latitude, 6) + "&lng=" + String(longitude, 6) + "&formatted=0";
 
  Serial.print("Requesting: ");
  Serial.println(path);
 
  // Send HTTP GET request
  sunClient.get(path);
 
  // Get status code and full response
  int statusCode = sunClient.responseStatusCode();
  String response = sunClient.responseBody();
 
  Serial.print("Status code: ");
  Serial.println(statusCode);
 
  Serial.println("---- JSON Response ----");
  Serial.println(response);
  Serial.println("------------------------");
 
  // Extract sunrise and sunset times from JSON
  String sunrise = extractValue(response, "\"sunrise\":\"", "\"");
  String sunset  = extractValue(response, "\"sunset\":\"", "\"");
 
  Serial.println("\n----- Extracted Sun Data -----");
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
 
// ----------------------------------------------------------
// Finds and returns a value inside a JSON-like string
// ----------------------------------------------------------
String extractValue(String data, String key, String delimiter) {
  int start = data.indexOf(key);
  if (start == -1) return "N/A";
  start += key.length();
  int end = data.indexOf(delimiter, start);
  if (end == -1) end = data.length();
  return data.substring(start, end);
}
 
// Neccessary because otherwise the space between the time and AM, or PM would cause an error
 
String urlEncode(String s) {
  s.replace(" ", "%20");
  return s;
}
 
// Convert ISO time to HH.MM float in order to display time as (8.27) for example
float timeToFloat(String isoTime) {
  int tIndex = isoTime.indexOf('T');
  if (tIndex == -1) return 0.0;
 
  String timePart = isoTime.substring(tIndex + 1, tIndex + 6); // "08:27"
  int hour = timePart.substring(0, 2).toInt();
  int minute = timePart.substring(3, 5).toInt();
 
  // Convert to HH.MM format
  return hour + (minute / 100.0);
}
 
 
 

// ---------- ThingSpeak POST ----------
void postToThingSpeak(float lux, float temperature, bool alarm, int sensorState) {
  if (WRITE_API_KEY.length() == 0) {
    Serial.println("ThingSpeak WRITE_API_KEY empty - skipping post.");
    return;
  }

  String body = "api_key=" + WRITE_API_KEY +
                "&field1=" + String(lux, 1) +
                "&field2=" + String(temperature, 1) +
                "&field3=" + String(alarm ? 1 : 0) +
                "&field4=" + String(sensorState);
                "&field5=" + String(sunriseFloat, 2) +   // numeric HH.MM
                "&field6=" + String(sunsetFloat, 2) +     // numeric HH.MM
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

// ---------- Blynk handlers for manual control ----------
BLYNK_WRITE(V8) {   // Manual time input "HH:MM"
  String t = param.asString();
  sscanf(t.c_str(), "%d:%d", &hours, &minutes);
  seconds = 0;
}

BLYNK_WRITE(V9) {   // Manual buzzer control
  buzzerActive = param.asInt();
}

BLYNK_WRITE(V10) {  // Manual LED control
  digitalWrite(LED_PIN, param.asInt());
}

BLYNK_WRITE(V3) {   // Manual button control from console: 0/1
  int v = param.asInt();
  if (v == 1) {
    // emulate local button press behaviour
    if (buzzerActive) {
      buzzerActive = false;
      Serial.println("Blynk V3: silenced buzzer.");
    } else {
      buzzerActive = true;
      alarmTriggered = true;
      resetClocks();
      currentAlarmCause = NONE;
      Serial.println("Blynk V3: activated buzzer.");
    }
  } else {
    // v == 0 -> turn off alarm entirely
    buzzerActive = false;
    alarmTriggered = false;
    currentAlarmCause = NONE;
    Serial.println("Blynk V3: deactivated buzzer/alarm.");
  }
}

// ---------- SETUP ----------
void setup() {
  Serial.begin(9600);
  
  analogReadResolution(12);  // Required for Arduino UNO R4
  delay(2000);
  Serial.println("Combined Atmosphere Warning System + ThingSpeak + Blynk");




  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT);    // assumes external pull-down; change to INPUT_PULLUP if using internal pull-up
  pinMode(TOUCH_PIN, INPUT);     // digital sensor trigger
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
  currentAlarmCause = NONE;

  // Start Blynk (this will manage WiFi connection)
  Blynk.begin(BLYNK_AUTH_TOKEN, SECRET_SSID, SECRET_PASS);

  // Wait for WiFi to be connected and print IP
  unsigned long startWait = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - startWait) < 15000UL) {
    Blynk.run(); // allow Blynk to handle connection
    delay(50);
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Connected to Wi-Fi. IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("Warning: WiFi not connected after timeout. ThingSpeak will be skipped until connection.");
  }

  // initialize timers
  previousMillis = millis();
  lcdUpdateMillis = previousMillis;
  thingspeakMillis = previousMillis;
  printUpdateMillis = previousMillis;
  displayCycleMillis = previousMillis;
}

// ---------- LOOP ----------
void loop() {
  unsigned long currentMillis = millis();

  // allow Blynk background tasks
  Blynk.run();

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
  tempC = readGroveTempC(TEMP_PIN);
  lightLux = readGroveLightLux(LIGHT_PIN);
  int digitalSensorState = digitalRead(TOUCH_PIN);

  // --- sensor-triggered alarms ---
  if (!buzzerActive) {
    if (tempC < TEMP_MIN) {
      buzzerActive = true;
      alarmTriggered = true;
      currentAlarmCause = TEMP_LOW;
      Serial.println("Temperature too low - activating alarm and resetting clocks.");
      Blynk.logEvent("alarm_event", "Temp too low");
      resetClocks();
    } else if (tempC > TEMP_MAX) {
      buzzerActive = true;
      alarmTriggered = true;
      currentAlarmCause = TEMP_HIGH;
      Serial.println("Temperature too high - activating alarm and resetting clocks.");
      Blynk.logEvent("alarm_event", "Temp too high");
      resetClocks();
    } else if (digitalSensorState == HIGH) {
      buzzerActive = true;
      alarmTriggered = true;
      currentAlarmCause = SENSOR_TRIGGER;
      Serial.println("Sensor triggered buzzer - resetting clocks.");
      Blynk.logEvent("alarm_event", "Sensor triggered");
      resetClocks();
    }
  }

  // --- button debounce & handling (local physical button) ---
  int reading = digitalRead(BUTTON_PIN);
  if (reading != lastButtonState) {
    lastDebounceTime = currentMillis;
  }
  if ((currentMillis - lastDebounceTime) > debounceDelay) {
    if (reading != buttonState) {
      buttonState = reading;
      // assuming button reads HIGH when pressed (external pull-down)
      if (buttonState == HIGH) {
        if (buzzerActive) {
          // Silence the buzzer but keep alarmTriggered true
          buzzerActive = false;
          Serial.println("Button silenced the alarm buzzer.");
        } else {
          // Normal toggle buzzer and reset if activating
          buzzerActive = !buzzerActive;
          if (buzzerActive) {
            Serial.println("Button activated buzzer - resetting clocks.");
            resetClocks();
            currentAlarmCause = NONE;
            alarmTriggered = true;
          } else {
            Serial.println("Button deactivated buzzer.");
            alarmTriggered = false;
            currentAlarmCause = NONE;
          }
        }
      }
    }
  }
  lastButtonState = reading;

  // --- LCD top row updates every second (time) ---
  if (currentMillis - lcdUpdateMillis >= lcdInterval) {
    lcdUpdateMillis += lcdInterval;

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

    // Also update Blynk clock/time virtual pins
    char timeBuf[10];
    sprintf(timeBuf, "%02d:%02d:%02d", hours, minutes, seconds);
    Blynk.virtualWrite(V7, timeBuf);
    // elapsed time
    char elapsedBuf[10];
    sprintf(elapsedBuf, "%02d:%02d:%02d", elapsedHours, elapsedMinutes, elapsedSeconds);
    Blynk.virtualWrite(V6, elapsedBuf);
  }

  // --- LCD bottom row ---
  if (buzzerActive) {
    // Blink warning every 500ms
    lcd.setCursor(0, 1);
    if ((currentMillis / 500) % 2 == 0) {
      switch (currentAlarmCause) {
        case TEMP_LOW:
          lcd.print(" Temp TOO LOW!   ");
          Blynk.virtualWrite(V11, "Temp TOO LOW!");
          break;
        case TEMP_HIGH:
          lcd.print(" Temp TOO HIGH!  ");
          Blynk.virtualWrite(V11, "Temp TOO HIGH!");
          break;
        case SENSOR_TRIGGER:
          lcd.print(" Sensor Trigger! ");
          Blynk.virtualWrite(V11, "Sensor Trigger!");
          break;
        default:
          lcd.print("   WARNING!      ");
          Blynk.virtualWrite(V11, "Warning!");
          break;
      }
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
      Blynk.virtualWrite(V11, "Running...");
    } else if (displayState == 1) {
      char buf[17];
      snprintf(buf, sizeof(buf), "Temp:%5.1f C      ", tempC);
      lcd.print(buf);
      lcd.setRGB(0, 150, 200);
      Blynk.virtualWrite(V11, buf);
    } else {
      char buf[17];
      snprintf(buf, sizeof(buf), "Light:%6.0f lx    ", lightLux);
      lcd.print(buf);
      lcd.setRGB(120, 100, 0);
      Blynk.virtualWrite(V11, buf);
    }
  }

  // ---------- Send sensor values to Blynk regularly (non-blocking) ----------
  // update virtuals every thingspeakInterval to reduce traffic
  if (currentMillis - thingspeakMillis >= thingspeakInterval) {
    // send to ThingSpeak first (if configured)
    if (WiFi.status() == WL_CONNECTED) {
      postToThingSpeak(lightLux, tempC, alarmTriggered, digitalSensorState);
    }
    // Update Blynk virtual pins
    Blynk.virtualWrite(V0, tempC);
    Blynk.virtualWrite(V1, lightLux);
    Blynk.virtualWrite(V2, buzzerActive ? 1 : 0);
    Blynk.virtualWrite(V5, digitalSensorState);

    thingspeakMillis += thingspeakInterval;
  }

  // --- buzzer and LED control ---
  if (buzzerActive) {
    int toneFreq = 1000;  // default
    switch (currentAlarmCause) {
      case TEMP_LOW: toneFreq = 500; break;
      case TEMP_HIGH: toneFreq = 1500; break;
      case SENSOR_TRIGGER: toneFreq = 1000; break;
      default: toneFreq = 1000; break;
    }
    tone(BUZZER_PIN, toneFreq);

    // LED flashes every 500 ms
    if ((currentMillis / 500) % 2 == 0) {
      digitalWrite(LED_PIN, HIGH);
    } else {
      digitalWrite(LED_PIN, LOW);
    }
  } else {
    noTone(BUZZER_PIN);
    digitalWrite(LED_PIN, LOW);
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
    Serial.print(digitalRead(TOUCH_PIN));
    Serial.print(" | Lux: ");
    Serial.print(lightLux, 1);
    Serial.print(" | Temp: ");
    Serial.print(tempC, 1);
    Serial.println(" C");
  }

  // loop remains non-blocking
}
