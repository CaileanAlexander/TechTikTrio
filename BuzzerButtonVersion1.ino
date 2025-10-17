const int buttonPin = 3; // Arduino pin connected to button's pin
const int buzzerPin = 8; // Arduino pin connected to Buzzer's pin

void setup() {
  Serial.begin(9600);                // initialize serial
  pinMode(buttonPin INPUT_PULLUP); // set arduino pin to input pull-up mode
  pinMode(buzzerPin, OUTPUT);       // set arduino pin to output mode
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