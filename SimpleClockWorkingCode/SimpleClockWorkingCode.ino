#include <Wire.h>
#include "rgb_lcd.h"

rgb_lcd lcd;

unsigned long previousMillis = 0;
int seconds = 0;
int minutes = 0;
int hours = 0;

void setup() {
  lcd.begin(16, 2);
  lcd.setRGB(0, 100, 255); // Nice blue backlight for the clock
  lcd.print("Simple Clock");
  delay(2000);
  lcd.clear();
}

void loop() {
  unsigned long currentMillis = millis();

  // Check if 1 second has passed
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

    // Display time on LCD
    lcd.setCursor(0, 0);
    lcd.print("Time:");

    lcd.setCursor(6, 0);

    // Format HH:MM:SS with leading zeros
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
}
