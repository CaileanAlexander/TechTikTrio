#include <Wire.h>
#include "rgb_lcd.h"

#define LED 6
#define tempPin A0
#define sensorPin 4  // External sensor pin

rgb_lcd lcd;

const int buzzerPin = 8;
const int buttonPin = 3;

bool buzzerActive = false;
bool alarmTriggered = false;

int lastButtonState = LOW;
int buttonState = LOW;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;

unsigned long previousMillis = 0;           // For clock
unsigned long lcdUpdateMillis = 0;          // For LCD updates
unsigned long printUpdateMillis = 0;        // For Serial printing
const unsigned long lcdUpdateInterval = 1000;  // 1 second
const unsigned long printInterval = 5000;      // 5 seconds

int seconds = 0;
int minutes = 0;
int hours = 0;

// For elapsed time since last reset
int elapsedSeconds = 0;
int elapsedMinutes = 0;
int elapsedHours = 0;

float temp = 0;

void setup() {
  Serial.begin(9600);

  pinMode(buzzerPin, OUTPUT);
  pinMode(buttonPin, INPUT);   // External pull-down or resistor setup
  pinMode(sensorPin, INPUT);   // Sensor input pin
  pinMode(LED, OUTPUT);

  lcd.begin(16, 2);
  lcd.setRGB(0, 100, 255);
  lcd.print("   TechTikTrio    ");
  lcd.setCursor(0, 1);
  lcd.print("   Atmosphere         Alarm     ");
  delay(2000);
  lcd.clear();

  noTone(buzzerPin);
  buzzerActive = false;
  alarmTriggered = false;

  Serial.println("Setup complete. Clock starting at 00:00:00");
}

void loop() {
  unsigned long currentMillis = millis();

  // ---- Update clock every second ----
  if (currentMillis - previousMillis >= 1000) {
    previousMillis = currentMillis;
    seconds++;
    elapsedSeconds++;

    if (seconds >= 60) {
      seconds = 0;
      minutes++;
    }
    if (minutes >= 60) {
      minutes = 0;
      hours++;
    }
    if (hours >= 24) {
      hours = 0;
    }

    // Update elapsed timer too
    if (elapsedSeconds >= 60) {
      elapsedSeconds = 0;
      elapsedMinutes++;
    }
    if (elapsedMinutes >= 60) {
      elapsedMinutes = 0;
      elapsedHours++;
    }
  }

  // ---- Read temperature (LM35) ----
  int rawTemp = analogRead(tempPin);
  temp = rawTemp * 0.48828125;

  // ---- Button Debounce ----
  int reading = digitalRead(buttonPin);
  if (reading != lastButtonState) {
    lastDebounceTime = currentMillis;
  }
  if ((currentMillis - lastDebounceTime) > debounceDelay) {
    if (reading != buttonState) {
      buttonState = reading;

      if (buttonState == HIGH) {  // Button pressed
        buzzerActive = !buzzerActive;
        if (buzzerActive) {
          Serial.println("Button activated buzzer - resetting clock and elapsed time");
          resetClock();
        } else {
          Serial.println("Button deactivated buzzer");
          alarmTriggered = false;
        }
      }
    }
  }
  lastButtonState = reading;

  // ---- Sensor Trigger ----
  int sensorState = digitalRead(sensorPin);
  if (sensorState == HIGH && !buzzerActive) {
    buzzerActive = true;
    alarmTriggered = true;
    Serial.println("Sensor triggered buzzer - resetting clock and elapsed time");
    resetClock();
  }

  // ---- LCD Updates every 1 second ----
  if (currentMillis - lcdUpdateMillis >= lcdUpdateInterval) {
    lcdUpdateMillis = currentMillis;

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

    lcd.setCursor(0, 1);
    if (buzzerActive) {
      if ((currentMillis / 500) % 2 == 0) {
        lcd.print("    WARNING!    ");
        lcd.setRGB(255, 0, 0);
      } else {
        lcd.print("                 ");
        lcd.setRGB(100, 0, 0);
      }
    } else {
      lcd.print("Running...       ");
      lcd.setRGB(0, 100, 255);
    }
  }

  // ---- Buzzer and LED Control ----
  if (buzzerActive) {
    tone(buzzerPin, 1000);
  } else {
    noTone(buzzerPin);
  }

  digitalWrite(LED, buzzerActive ? HIGH : LOW);

  // ---- Serial Print every 5 seconds ----
  if (currentMillis - printUpdateMillis >= printInterval) {
    printUpdateMillis = currentMillis;

    Serial.print("Clock Time: ");
    printTime(hours, minutes, seconds);
    Serial.print(" | Elapsed Since Reset: ");
    printTime(elapsedHours, elapsedMinutes, elapsedSeconds);
    Serial.print(" | BuzzerActive: ");
    Serial.print(buzzerActive);
    Serial.print(" | Sensor: ");
    Serial.print(sensorState);
    Serial.print(" | Temp: ");
    Serial.print(temp);
    Serial.println(" C");
  }
}

// ---- Helper: Reset both timers ----
void resetClock() {
  seconds = 0;
  minutes = 0;
  hours = 0;
  elapsedSeconds = 0;
  elapsedMinutes = 0;
  elapsedHours = 0;
}

// ---- Prints time elapsed from buzzer sounding ----
void printTime(int h, int m, int s) {
  if (h < 10) Serial.print('0');
  Serial.print(h);
  Serial.print(':');
  if (m < 10) Serial.print('0');
  Serial.print(m);
  Serial.print(':');
  if (s < 10) Serial.print('0');
  Serial.print(s);
}
