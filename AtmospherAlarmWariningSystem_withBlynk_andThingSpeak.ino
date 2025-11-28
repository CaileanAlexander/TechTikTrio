/*
  Combined Smart Clock + Sensors + ThingSpeak POST + Blynk Integration
  - Temp sensor (LM35): A0
  - Light sensor: A1
  - External digital sensor trigger: D4
  - Buzzer: D8
  - Button (debounced): D3
  - LED: D6
  - RGB LCD: I2C (rgb_lcd)
  - ThingSpeak POST every 20 seconds
  - Blynk virtual pins for sensors, controls, messages, time
  - Blynk events triggered on alarms
*/

#include <Wire.h>
#include "rgb_lcd.h"
#include <WiFiS3.h>
#include <ArduinoHttpClient.h>
#include <BlynkSimpleEsp32_S3.h>  // Adjust for your Arduino Rev4 / WiFi board
#include "arduino_secrets.h"      // SECRET_SSID, SECRET_PASS, BLYNK_AUTH, THINGSPEAK_API_KEY

// ---------- ThingSpeak / Wi-Fi config ----------
const char THINGSPEAK_SERVER[] = "api.thingspeak.com";
const int THINGSPEAK_PORT = 80;
const String WRITE_API_KEY = THINGSPEAK_API_KEY;

WiFiClient wifiClient;
HttpClient httpClient(wifiClient, THINGSPEAK_SERVER, THINGSPEAK_PORT);

// ---------- Blynk config ----------
char auth[] = BLYNK_AUTH;  // Your Blynk auth token

// ---------- Pins ----------
#define LED_PIN 6
#define TEMP_PIN A0     // LM35 temperature sensor
#define LIGHT_PIN A1    // Grove light sensor
#define SENSOR_PIN 4    // External digital sensor trigger (D4)
const int BUZZER_PIN = 8;
const int BUTTON_PIN = 3;

// ---------- LCD ----------
rgb_lcd lcd;

// ---------- State variables ----------
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

// display cycle state: 0=Running,1=Temp,2=Light
int displayState = 0;

// External API message (simulate or update dynamically)
String externalApiMessage = "";

// ---------------- Sensor helper functions ----------------
float readLm35C(int pin) {
  int raw = analogRead(pin);
  return raw * 0.48828125f;  // LM35 10mV per °C, ADC step ~4.887mV
}

float readGroveLightLux(int pin) {
  int sensorValue = analogRead(pin);
  if (sensorValue <= 0) return 0;
  float resistance = (1023.0 - sensorValue) * 10.0 / sensorValue; // kΩ approx
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

// ---------- Blynk Virtual Pin Handlers ----------
// Temperature (V0)
BLYNK_WRITE(V8) {
  // Manual time setting input as string "HH:MM:SS"
  String timeInput = param.asString();
  int h = 0, m = 0, s = 0;
  sscanf(timeInput.c_str(), "%d:%d:%d", &h, &m, &s);
  if (h >= 0 && h < 24 && m >= 0 && m < 60 && s >= 0 && s < 60) {
    hours = h; minutes = m; seconds = s;
    Serial.print("Manual time set to: ");
    printTimeSerial(hours, minutes, seconds);
    Serial.println();
  }
}
// Manual buzzer control (V9)
BLYNK_WRITE(V9) {
  int val = param.asInt();
  buzzerActive = (val != 0);
  if (!buzzerActive) alarmTriggered = false;
}
// Manual LED control (V10)
BLYNK_WRITE(V10) {
  int val = param.asInt();
  digitalWrite(LED_PIN, val ? HIGH : LOW);
}
// Button Control (V3)
BLYNK_WRITE(V3) {
  int val = param.asInt();
  buzzerActive = (val != 0);
  if (!buzzerActive) alarmTriggered = false;
}

// ---------- Setup ----------
void setup() {
  Serial.begin(9600);
  delay(2000);
  Serial.println("Combined Smart Clock + ThingSpeak + Blynk");

  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT);
  pinMode(SENSOR_PIN, INPUT);
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

  // Connect WiFi (for ThingSpeak & Blynk)
  WiFi.begin(SECRET_SSID, SECRET_PASS);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  // Connect Blynk
  Blynk.begin(auth, SECRET_SSID, SECRET_PASS);

  // Initialize timers
  previousMillis = millis();
  lcdUpdateMillis = previousMillis;
  thingspeakMillis = previousMillis;
  printUpdateMillis = previousMillis;
  displayCycleMillis = previousMillis;
}

// ---------- Main Loop ----------
void loop() {
  Blynk.run();

  unsigned long currentMillis = millis();

  // Clock update every 1 second
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

  // Read sensors
  tempC = readLm35C(TEMP_PIN);
  lightLux = readGroveLightLux(LIGHT_PIN);
  int digitalSensorState = digitalRead(SENSOR_PIN);

  // Temperature threshold alarm (10-27 °C)
  if ((tempC < 10 || tempC > 27) && !buzzerActive) {
    buzzerActive = true;
    alarmTriggered = true;
    Serial.println("Temperature alarm triggered!");
    Blynk.logEvent("temp_alarm", "Temperature out of range: " + String(tempC,1));
  }

  // External digital sensor alarm
  if (digitalSensorState == HIGH && !buzzerActive) {
    buzzerActive = true;
    alarmTriggered = true;
    Serial.println("External sensor triggered alarm!");
    Blynk.logEvent("button_press", "External sensor triggered alarm.");
  }

  // Button debounce & handling
  int reading = digitalRead(BUTTON_PIN);
  if (reading != lastButtonState) lastDebounceTime = currentMillis;
  if ((currentMillis - lastDebounceTime) > debounceDelay) {
    if (reading != buttonState) {
      buttonState = reading;
      if (buttonState == HIGH) {
        buzzerActive = !buzzerActive;
        if (buzzerActive) {
          alarmTriggered = true;
          Serial.println("Button pressed - alarm ON");
          Blynk.logEvent("button_press", "Button pressed - alarm ON");
          resetClocks();
        } else {
          alarmTriggered = false;
          Serial.println("Button pressed - alarm OFF");
        }
      }
    }
  }
  lastButtonState = reading;

  // LCD top row update every second
  if (currentMillis - lcdUpdateMillis >= lcdInterval) {
    lcdUpdateMillis += lcdInterval;

    lcd.setCursor(0, 0);
    lcd.print("Time:");
    if (hours < 10) lcd.print('0');
    lcd.print(hours);
    lcd.print(':');
    if (minutes < 10) lcd.print('0');
    lcd.print(minutes);
    lcd.print(':');
    if (seconds < 10) lcd.print('0');
    lcd.print(seconds);
  }

  // LCD bottom row
  lcd.setCursor(0, 1);
  if (buzzerActive) {
    // Warning message + flashing red backlight
    if ((currentMillis / 500) % 2 == 0) {
      lcd.print("  !!! ALARM !!!  ");
      lcd.setRGB(255, 0, 0);
    } else {
      lcd.print("                ");
      lcd.setRGB(100, 0, 0);
    }
  } else {
    if (currentMillis - displayCycleMillis >= displayCycleInterval) {
      displayCycleMillis += displayCycleInterval;
      displayState = (displayState + 1) % 3; // cycle 0..2
    }

    if (displayState == 0) {
      lcd.print("Running...       ");
      lcd.setRGB(0, 100, 255);
    } else if (displayState == 1) {
      char buf[17];
      snprintf(buf, sizeof(buf), "Temp:%5.1f C      ", tempC);
      lcd.print(buf);
      lcd.setRGB(0, 150, 200);
    } else {
      char buf[17];
      snprintf(buf, sizeof(buf), "Light:%6.0f lx    ", lightLux);
      lcd.print(buf);
      lcd.setRGB(120, 100, 0);
    }
  }

  // Buzzer and LED control
  if (buzzerActive) {
    tone(BUZZER_PIN, 1000);
    // Flash LED every 500ms
    if ((currentMillis / 500) % 2 == 0) digitalWrite(LED_PIN, HIGH);
    else digitalWrite(LED_PIN, LOW);
  } else {
    noTone(BUZZER_PIN);
    digitalWrite(LED_PIN, LOW);
  }

  // ThingSpeak upload every 20 seconds
  if (currentMillis - thingspeakMillis >= thingspeakInterval) {
    thingspeakMillis += thingspeakInterval;
    postToThingSpeak(lightLux, tempC, buzzerActive, digitalSensorState);
  }

  // Serial debug every 5 seconds
  if (currentMillis - printUpdateMillis >= printInterval) {
    printUpdateMillis += printInterval;
    Serial.print("Clock: ");
    printTimeSerial(hours, minutes, seconds);
    Serial.print(" | Elapsed: ");
    printTimeSerial(elapsedHours, elapsedMinutes, elapsedSeconds);
    Serial.print(" | Buzzer: ");
    Serial.print(buzzerActive ? "ON" : "OFF");
    Serial.print(" | Sensor: ");
    Serial.print(digitalSensorState);
    Serial.print(" | Lux: ");
    Serial.print(lightLux, 1);
    Serial.print(" | Temp: ");
    Serial.print(tempC, 1);
    Serial.println(" C");
  }
}
