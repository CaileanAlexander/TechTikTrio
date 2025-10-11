//Code to control alarm clock using buzzer that sounds after X amount of time, and button that turns buzzer off.

const int buzzerPin = A0; // Buzzer is connected to A0
const int buttonPin = 3; // Button is connected to D3
const int lightsensorPin = 4; // Light sensor pin is connected to D4
const int temperaturePin = 5; // Temperature pin is connected to D5

const int lightThreshold = 300; // Lower than this and the plants do not have enough light to grow in their current position
// The buzzer will sound when the light threshold detected by the sensor is below this in order to alert the farmer to change the position of the plant.


void setup() {
  // put your setup code here, to run once:
  pinMode(buzzerPin, OUTPUT);
  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(lightsensorPin, INPUT);
  pinMode(temperaturePin, INPUT);
  Serial.begin(9600); // Debugging for this project

  digitalwrite(buzzerPin, LOW); //This is defining that we want the buzzer inital state at Setup to be LOW, i.e. not making any sound
}


void loop() {
  // Reads button state
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
  

  int lightValue = analogRead(lightSensorPin);

  Serial.print("Light Sensor Reading: ")
  Serial.println(lightValue);

  // Turn on the buzzer at slowest speed when the light level is below the threshold
  if (lightValue < lightThreshold) {
    digitalWrite(buzzerPin, HIGH); // Turns on the buzzer
    tone(BUZZER, 85); // Tone is a new function which works with the Buzzer. We have to pass in what we are using (A0 for the buzzer).
    delay(3000); //3000 milliseconds = 3 seconds
    Serial.println("The current position of the plant is not providing sufficient light. Move the plant to an area of more light");
  }
  else{
    digitalWrite(buzzerPin, LOW); //Turns off the Buzzer
    Serial.println("The current position of the plant is obtaining a suitable amount of light.");
  }
}