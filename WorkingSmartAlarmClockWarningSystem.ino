#include <Wire.h>
#include "rgb_lcd.h"

#define LED 6
#define tempPin A0

rgb_lcd lcd;

const int buzzerPin = 8;
const int buttonPin = 3;

bool buzzerActive = false;
bool alarmTriggered = false;

int lastButtonState = LOW;    // Last reading from the button pin
int buttonState = LOW;        // Current stable button state
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;

unsigned long previousMillis = 0;

int seconds = 0;
int minutes = 0;
int hours = 0;

float temp = 0;

void setup() {
  Serial.begin(9600);

  pinMode(buzzerPin, OUTPUT);
  pinMode(buttonPin, INPUT);  // No pull-up resistor enabled

  pinMode(LED, OUTPUT);

  lcd.begin(16, 2);
  lcd.setRGB(0, 100, 255);
  lcd.print("   TechTikTrio    ");
  lcd.setCursor(0, 1);
  lcd.print("Smart Clock      ");
  delay(2000);
  lcd.clear();

  noTone(buzzerPin);
  buzzerActive = false;
  alarmTriggered = false;

  Serial.println("Setup complete. Starting clock at 00:00:00");
}

void loop() {
  unsigned long currentMillis = millis();

  // Update clock every second
  if (currentMillis - previousMillis >= 1000) {
    previousMillis = currentMillis;
    seconds++;

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

    // Alarm triggers once at 1:00
    if (minutes == 1 && seconds == 0 && !alarmTriggered) {
      alarmTriggered = true;
      buzzerActive = true;
      Serial.println("Automatic buzzer ON at 1:00");
    }
  }

  // Read temperature sensor (unused now)
  int rawTemp = analogRead(tempPin);
  temp = rawTemp * 0.48828125;

  // Display time on LCD line 1
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

  // Button reading and debounce
  int reading = digitalRead(buttonPin);
  Serial.print("Button reading: ");
  Serial.println(reading);

  if (reading != lastButtonState) {
    lastDebounceTime = currentMillis;
  }

  if ((currentMillis - lastDebounceTime) > debounceDelay) {
    if (reading != buttonState) {
      buttonState = reading;

      if (buttonState == HIGH) {  // Button pressed
        buzzerActive = !buzzerActive;
        if (!buzzerActive) {
          alarmTriggered = false;
        }
        Serial.print("Button pressed! Buzzer toggled to: ");
        Serial.println(buzzerActive ? "ON" : "OFF");
      }
    }
  }

  lastButtonState = reading;

  // LCD line 2 messages and backlight
  lcd.setCursor(0, 1);
  if (buzzerActive) {
    if ((currentMillis / 500) % 2 == 0) {
      lcd.print("   Wake Up!      ");
      lcd.setRGB(255, 0, 0);
    } else {
      lcd.print("                 ");
      lcd.setRGB(100, 0, 0);
    }
  } else {
    lcd.print("Running...       ");
    lcd.setRGB(0, 100, 255);
  }

  // Buzzer control
  if (buzzerActive) {
    tone(buzzerPin, 1000);
  } else {
    noTone(buzzerPin);
  }

  // LED indicator
  digitalWrite(LED, buzzerActive ? HIGH : LOW);

  // Debug output
  Serial.print("Time: ");
  Serial.print(hours);
  Serial.print(":");
  Serial.print(minutes);
  Serial.print(":");
  Serial.print(seconds);
  Serial.print(" | BuzzerActive: ");
  Serial.println(buzzerActive);

  delay(10);
}
