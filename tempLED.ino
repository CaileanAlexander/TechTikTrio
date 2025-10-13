#define LED 6
float temp;
int tempPin = 0;

const float thresholdTemp = 5.0;  // Threshold at 5°C

void setup() {
   Serial.begin(9600);
   pinMode(LED, OUTPUT);
}

void loop() {
   temp = analogRead(tempPin);
   temp = temp * 0.48828125;

   Serial.print("TEMPERATURE = ");
   Serial.print(temp);
   Serial.println("*C");

   if (temp < thresholdTemp) {
      digitalWrite(LED, HIGH);  // LED ON if temp is below 5°C
   } else {
      digitalWrite(LED, LOW);   // LED OFF otherwise
   }

   delay(1000);
}
