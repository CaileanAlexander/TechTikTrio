const int touchsensorPin=7; // The touch sensor pin is connected to the digital pin D7.

int lasttouchsensorState = LOW; // Previous reading from the touch sensor pin
int currenttouchsensorState; // Tracks the current touch sensor state

unsigned long startTime = 0;    // When sensor was touched
bool timerRunning = false;      // Track if timer is active

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  pinMode(touchsensorPin,INPUT);
}

void loop() {
  currenttouchsensorState = digitalRead(touchsensorPin);
  
  if(lasttouchsensorState == LOW && currenttouchsensorState == HIGH){
  Serial.println("Touch sensor activated- someone has entered.");
  startTime=millis();
  timerRunning=true;
  }

  if (timerRunning){
    unsigned long elapsed = millis() - startTime;
    Serial.print("Seconds since someone has entered: ");
    Serial.println(elapsed / 1000); // Converts milliseconds to seconds
  }

  // Saves the last state
  lasttouchsensorState = currenttouchsensorState;
}
