// This will cause the LCD screen to show a Warning sign and blink 
lcd.setCursor(0,1);
if (buzzerActive) {
  // Create the lcd backlight to blink every half second
  if ((currentMillis / 500) % 2 == 0) {
    lcd.print("Warning!Warning!");
  } else {
    lcd.print("                "); // Warning sign disapears to create the blinking effect
  }
} lcd.print("Running...      "); // Message when alarm/Buzzer is off
}
