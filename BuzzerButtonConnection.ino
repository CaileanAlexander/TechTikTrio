//Code to control alarm clock using buzzer that sounds after X amount of time, and button that turns buzzer off.

const int buzzerPin = A0; // Buzzer is connected to A0
const int buttonPin = 3; // Button is connected to D3


void setup() {
  // put your setup code here, to run once:
  pinMode(buzzerPin, OUTPUT);
  pinMode(buttonPin, INPUT_PULLUP);
  Serial.begin(9600); // Debugging for this project
  Serial.println("Alarm sounding now..."); // Message printed to the Serial Monitor
}


void loop() {
  int buttonState = digitalRead(buttonPin); // read new state

  if (buttonState == LOW) {
    Serial.println("The button is being pressed");
    digitalWrite(buzzerPin, HIGH); // turn on
  }
  else
  if (buttonState == HIGH) {
    Serial.println("The button is unpressed");
    digitalWrite(buzzerPin, LOW);  // turn off
  }
}