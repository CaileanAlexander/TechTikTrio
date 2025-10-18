
#include <Wire.h> //Allows communication through a wire between the onboard library and the LCD Screen
#include "rgb_lcd.h" // enables comunication between library and LCD
#define LED 6 //def6ined port 6 for the LED module connection
float temp;
int tempPin = 0;

const float thresholdTemp = 20.0;  // Threshold at 20°C

rgb_lcd lcd; // enables you to control the LCD screen with code by defining the LCD Screen

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
  lcd.print("   TechTikTrio      Smart Clock  "); //Initialisation message our Group name on startup
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
  pinMode(LED, OUTPUT);    // Sets LED pin as output
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

  // --- Second line messages based on conditions ---
  lcd.setCursor(0, 1);

  //  If buzzer is active (alarm time)
  if (buzzerActive) {
  // Blink the "Wake Up!" message every 500 ms
  if ((currentMillis / 500) % 2 == 0) {
    lcd.print("   Wake Up!       ");
    lcd.setRGB(255, 0, 0); // Red backlight
  } else {
    lcd.print("                  "); // Clear line for blink effect
    lcd.setRGB(100, 0, 0); // Dim red between flashes
  }
}

//  If temperature exceeds threshold (plants need water)
  else if (temp > thresholdTemp) {
  lcd.print("Plants need water!");
  lcd.setRGB(255, 150, 0); // Orange backlight for temperature warning
}

//  Otherwise (normal running)
  else {
  lcd.print("Running...        ");
  lcd.setRGB(0, 100, 255); // Blue backlight for normal mode
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

 temp = analogRead(tempPin);
  temp = temp * 0.48828125;  // Convert analog value to temperature
  Serial.print("TEMPERATURE = ");
  Serial.print(temp);
  Serial.println("*C");

}
