#include <Wire.h>
#include "rgb_lcd.h"

rgb_lcd lcd;

const int buzzerPin = 8;   // Grove Buzzer on port D8 (changed Thursday 16th October)
const int buttonPin = 3;   // Grove Button on port D3

bool buzzerActive=false;      // Buzzer off initially
bool alarmTriggered = false;    // Track if alarm has sounded at 1:00
bool lastButtonState = HIGH;    // Track last button state

unsigned long previousMillis = 0;  // Track last time update

int seconds = 0;
int minutes = 0;
int hours = 0;

void setup() {
  Serial.begin(9600);

  pinMode(buzzerPin, OUTPUT);
  pinMode(buttonPin, INPUT_PULLUP);

  // initialize the lcd screeen
  lcd.begin(16, 2); 
  lcd.setRGB(0, 100, 255); // Blue backlight for the lcd screen
  lcd.print("TechTikTrio Smart Clock"); //Initialisation message our Group name on startup
  delay(2000);
  lcd.clear();

  // Initialize time variables explicitly
  seconds = 0;
  minutes = 0;
  hours = 0;

  // Make sure buzzer is OFF at start
  noTone(buzzerPin);
  buzzerActive = false;
  alarmTriggered = false;

  Serial.println("Setup complete. Starting clock at 00:00:00");
}

void loop() {
  unsigned long currentMillis = millis();

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

    // *** Alarm triggers ONLY at exactly 1:00 ***
    if (minutes == 1 && seconds == 0 && !alarmTriggered) {
      alarmTriggered = true;
      buzzerActive = true;
      Serial.println("Automatic buzzer ON at 1:00");
    }

    // Display time on LCD
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
    lcd.print("Running...     "); // Clear rest of line
  }

  // Handle button press to toggle buzzer ON/OFF manually
  int buttonState = digitalRead(buttonPin);

  if (buttonState == LOW && lastButtonState == HIGH) {
    buzzerActive = !buzzerActive;  // Toggle buzzer state between on and off when a particular action happens, e.g. a press of the button.
    Serial.println("Button pressed! Toggling buzzer.");
  }
  lastButtonState = buttonState;
  delay(50);  // debounce delay

  // Control buzzer tone based on buzzerActive
  if (buzzerActive) {
    tone(buzzerPin, 1000);
  } else {
    noTone(buzzerPin);
  }

  // Debug prints
  Serial.print("Time: ");
  Serial.print(hours);
  Serial.print(":");
  Serial.print(minutes);
  Serial.print(":");
  Serial.println(seconds);
  Serial.print("BuzzerActive: ");
  Serial.print(buzzerActive);
  Serial.print(", AlarmTriggered: ");
  Serial.println(alarmTriggered);
}
