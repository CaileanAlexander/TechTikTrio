#define BLYNK_TEMPLATE_ID "TMPL4IUVgIjiU"
#define BLYNK_TEMPLATE_NAME "Atmosphere Warning System"
#define BLYNK_AUTH_TOKEN ""

#include <Wire.h>
#include "rgb_lcd.h"
#include <WiFiS3.h>
#include <BlynkSimpleWiFiS3.h>

#include "arduino_secrets.h"

// ---------- Pins ----------
#define LED_PIN 6
#define TEMP_PIN A0
#define LIGHT_PIN A1
#define TOUCH_PIN 4
#define BUZZER_PIN 8
#define BUTTON_PIN 3

// ---------- LCD ----------
rgb_lcd lcd;

// ---------- State ----------
bool buzzerActive = false;
bool alarmTriggered = false;

// ---------- Clock ----------
unsigned long previousMillis = 0;
int seconds = 0, minutes = 0, hours = 0;

// ---------- Sensor Values ----------
float tempC = 0;
float lightLux = 0;

// ---------- Virtual Pins ----------
/*
V0  = Temperature
V1  = Light
V2  = Alarm State
V3  = Button Control
V4  = External API Message
V5  = External Sensor State
V6  = Time Elapsed
V7  = Clock Time
V8  = Manual Time Input
V9  = Manual Buzzer Control
V10 = Manual LED Control
V11 = LCD Display Text
*/

// ---------- Sensor Functions ----------
float readLm35C(int pin) {
  int raw = analogRead(pin);
  return raw * 0.48828125;
}

float readLightLux(int pin) {
  int val = analogRead(pin);
  if (val <= 0) return 0;
  float resistance = (1023.0 - val) * 10.0 / val;
  return 10000.0 / pow(resistance, 4.0 / 3.0);
}

// ---------- BLYNK INPUTS ----------

BLYNK_WRITE(V8) {   // Manual time input "HH:MM"
  String t = param.asString();
  sscanf(t.c_str(), "%d:%d", &hours, &minutes);
  seconds = 0;
}

BLYNK_WRITE(V9) {   // Manual buzzer
  buzzerActive = param.asInt();
}

BLYNK_WRITE(V10) {  // Manual LED
  digitalWrite(LED_PIN, param.asInt());
}

// ---------- SETUP ----------
void setup() {
  Serial.begin(9600);

  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT);
  pinMode(TOUCH_PIN, INPUT);

  lcd.begin(16, 2);
  lcd.setRGB(0, 150, 255);
  lcd.print("UNO R4 ONLINE");

  WiFi.begin(SECRET_SSID, SECRET_PASS);
  Blynk.begin(BLYNK_AUTH, SECRET_SSID, SECRET_PASS);
}

// ---------- LOOP ----------
void loop() {
  Blynk.run();

  // Clock tick
  if (millis() - previousMillis >= 1000) {
    previousMillis = millis();
    seconds++;
    if (seconds >= 60) { seconds = 0; minutes++; }
    if (minutes >= 60) { minutes = 0; hours++; }
    if (hours >= 24) hours = 0;
  }

  // Read Sensors
  tempC = readLm35C(TEMP_PIN);
  lightLux = readLightLux(LIGHT_PIN);
  int touchState = digitalRead(TOUCH_PIN);

  // ---------- Alarm Logic ----------
  if ((tempC < 10 || tempC > 27 || touchState == HIGH) && !buzzerActive) {
    buzzerActive = true;
    alarmTriggered = true;
    Blynk.logEvent("alarm_event", "Alarm Triggered");
  }

  // ---------- Buzzer & LED ----------
  if (buzzerActive) {
    tone(BUZZER_PIN, 1000);
    digitalWrite(LED_PIN, millis() / 500 % 2);
  } else {
    noTone(BUZZER_PIN);
    digitalWrite(LED_PIN, LOW);
  }

  // ---------- LCD ----------
  lcd.setCursor(0, 0);
  lcd.print("T:");
  lcd.print(tempC, 1);
  lcd.print("C ");

  lcd.setCursor(0, 1);
  lcd.print("L:");
  lcd.print(lightLux, 0);
  lcd.print("lx ");

  // ---------- SEND TO BLYNK ----------
  Blynk.virtualWrite(V0, tempC);
  Blynk.virtualWrite(V1, lightLux);
  Blynk.virtualWrite(V2, buzzerActive);
  Blynk.virtualWrite(V5, touchState);

  char timeBuf[10];
  sprintf(timeBuf, "%02d:%02d:%02d", hours, minutes, seconds);
  Blynk.virtualWrite(V7, timeBuf);
  Blynk.virtualWrite(V11, "System Running");
}

