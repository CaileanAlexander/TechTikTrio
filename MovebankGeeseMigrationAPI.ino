// -----------------------------------------------------------
//  Libraries
// -----------------------------------------------------------
//  WiFiS3.h handles the Wi-Fi connection for the UNO R4 WiFi board.
//  This library talks directly to the ESP32-S3 chip built into the board.
#include <WiFiS3.h>

//  ArduinoHttpClient.h allows you to send and receive data from web servers
//  using HTTP (GET, POST, etc.).  We'll use it to call Movebank.
#include <ArduinoHttpClient.h>

//  arduino_secrets.h is a small file (you create it yourself) that stores
//  your Wi-Fi SSID and password so they’re not visible in your main sketch.
#include "arduino_secrets.h"

//  rgb_lcd.h controls the Grove LCD RGB Backlight display.
//  It can show two lines of text and change backlight color (RGB).
#include "rgb_lcd.h"

// -----------------------------------------------------------
//  Movebank settings
// -----------------------------------------------------------
//  Replace this with the study ID on the selected study from Movebank
//  You can also add an individual ID to examine a specific animal.
const char* studyId = "31987083"; // This is where you add the Study ID
const char* individualId   = " ";  // This is where you specify a particular animal

// -----------------------------------------------------------
//  Wi-Fi credentials (loaded from arduino_secrets.h)
// -----------------------------------------------------------
//  Example content of arduino_secrets.h:
//    #define SECRET_SSID "YourWiFiName"
//    #define SECRET_PASS "YourWiFiPassword"
char ssid[] = SECRET_SSID;   // Wi-Fi network name
char pass[] = SECRET_PASS;   // Wi-Fi password

// -----------------------------------------------------------
//  Create the main objects used in the sketch
// -----------------------------------------------------------
//  WiFiClient handles the physical network connection.
WiFiClient wifi;

//  HttpClient sends the HTTP request to the weather server.
//  Host: movebank.org, Port: 80 (HTTP)
HttpClient client(wifi, "www.movebank.org", 80);

//  rgb_lcd is the object that controls the Grove LCD screen.
rgb_lcd lcd;

// -----------------------------------------------------------
//  setup()  → runs once at power-on or reset
// -----------------------------------------------------------
void setup() {
  Serial.begin(9600);            // Start Serial Monitor for debug messages
  while (!Serial);               // Wait for Serial Monitor to open (optional)

  // Initialize the LCD: 16 columns × 2 rows
  lcd.begin(16, 2);

  // Set backlight color to light blue while connecting
  lcd.setRGB(0, 100, 255);

   // Display startup message on LCD
  lcd.print("Connecting WiFi");

  // Try to connect to the Wi-Fi network
  connectWiFi();

  // Once connected, show confirmation message briefly
  lcd.clear();
  lcd.print("WiFi Connected!");
  delay(1500);                   // Pause 1.5 s so the user can read it
}

// -----------------------------------------------------------
//  loop()  → runs repeatedly after setup()
// -----------------------------------------------------------
void loop() {
  Serial.println("\nFetching Bird Migration Data...");
  getMovebankData();              // Get new data from the API and show it
  delay(60000);                   // Wait 60 seconds before refreshing again
}

// -----------------------------------------------------------
//  connectWiFi() → connects the UNO R4 WiFi to your router
// -----------------------------------------------------------
void connectWiFi() {
  // Begin connection attempt
  WiFi.begin(ssid, pass);

  // Wait here until the board reports it’s connected
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);                 // Check every second
    Serial.print(".");           // Print dots so you know it's trying
  }

  // Once connected, print details to Serial
  Serial.println("\nWi-Fi connected.");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP()); // Shows the board’s assigned IP address
}

// -----------------------------------------------------------
//  calculateDistance()
//  Standalone helper function using the Haversine formula
//  to calculate the distance between two coordinates
// -----------------------------------------------------------
float calculateDistance(float lat1, float lon1, float lat2, float lon2) {
  const float R = 6371.0; // Earth radius in km
  float dLat = radians(lat2 - lat1);
  float dLon = radians(lon2 - lon1);
  
  float a = sin(dLat / 2) * sin(dLat / 2) +
            cos(radians(lat1)) * cos(radians(lat2)) *
            sin(dLon / 2) * sin(dLon / 2);
  
  float c = 2 * atan2(sqrt(a), sqrt(1 - a));
  
  float distance = R * c;
  return distance;
}

// -----------------------------------------------------------
//  getMovebankData() → sends HTTP request, parses JSON, updates LCD
// -----------------------------------------------------------
void getMovebankData() {
  // Build the URL path for Movebank:
  // Example: /movebank/service/public/json?study_id=2911040
  String path = "/movebank/service/public/json?study_id=" + String(studyId);

  // To filter by individual bird (optional)
  // String path = "/movebank/service/public/json?study_id=" + String(studyId) +
  //               "&individual_id=" + String(individualID);

  // Send the HTTP GET request to the server
  client.get(path);

  // Read the numeric status (200 = OK)
  int statusCode = client.responseStatusCode();

  // Read the entire response body (the JSON data)
  String response = client.responseBody();

  // Print to Serial so you can see the raw JSON
  Serial.print("Status code: ");
  Serial.println(statusCode);
  Serial.println("---- JSON Response ----");
  Serial.println(response);
  Serial.println("------------------------");

  // -----------------------------------------------------------
  //  Extract values from the columns in the CSV file (dummy data for now)
  // -----------------------------------------------------------
  String birdName    = "Goose123";  // individual-local-identifier
  float gooseLat     = 53.55;       // location-lat
  float gooseLong    = -8.05;       // location-long

  const float farmLat = 53.5;
  const float farmLon = -8.0;

  // Calculate distance
  float distance = calculateDistance(farmLat, farmLon, gooseLat, gooseLong);

  // -----------------------------------------------------------
  //  Set LCD backlight color according to risk level
  // -----------------------------------------------------------
  if (distance < 5.0)        lcd.setRGB(255, 0, 0);     // Red- geese very close.
  else if (distance < 20.0)  lcd.setRGB(255, 255, 0);   // Yellow- geese moderately close.
  else                        lcd.setRGB(0, 255, 0);     // Green: geese far away

  // -----------------------------------------------------------
  //  Prepare the text to show on LCD
  // -----------------------------------------------------------
  lcd.clear();                     
  lcd.setCursor(0, 0);             
  lcd.print(birdName);             

  String line2 = "Distance: " + String(distance, 1) + "km"; 
  lcd.setCursor(0, 1);
  lcd.print(line2);

  // -----------------------------------------------------------
  //  Print readable info to Serial (debug)
  // -----------------------------------------------------------
  Serial.println("\n----- Goose Data -----");
  Serial.print("Bird Name: ");
  Serial.println(birdName);
  Serial.print("Distance to farm: ");
  Serial.print(distance, 1);
  Serial.println(" km");
  Serial.println("----------------------------------");
}
