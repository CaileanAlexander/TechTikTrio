// Include the Wi-Fi library made for the UNO R4 WiFi board.
// It allows the board to connect to the internet using its built-in ESP32-S3 chip.
#include <WiFiS3.h>

// Include the Arduino HTTP Client library.
// This makes it easy to send HTTP requests (like GET and POST) to web servers.
#include <ArduinoHttpClient.h>

// Include a separate file that stores your Wi-Fi name and password.
// This keeps your credentials private if you share the sketch.
#include "arduino_secrets.h"

// After research, our team determined that there was not a relevant API to our project available
// We have taken the initiative instead to use Agriland.ie's XML data, and adapt the Open Weather Map code to this different format.

const char* rssHost = "www.agriland.ie"; // Names the host of Agriland 
const char* rssPath = "/feed/";

// --- Wi-Fi credentials ---
// These come from the arduino_secrets.h file.
// Example content of that file:
//   #define SECRET_SSID "YourWiFiName"
//   #define SECRET_PASS "YourWiFiPassword"
char ssid[] = SECRET_SSID;   // Wi-Fi network name (SSID)
char pass[] = SECRET_PASS;   // Wi-Fi password

// --- Wi-Fi and HTTP setup objects ---
// WiFiClient handles the low-level network connection.
WiFiSSLClient wifi;

// HttpClient handles the actual HTTP requests to the web API.
// Here we connect to api.openweathermap.org on port 80 (HTTP).
HttpClient client(wifi, "www.agriland.ie", 443);



void setup() {
  Serial.begin(9600);             // Start the serial monitor so we can print messages
  while (!Serial);                // Wait until the Serial Monitor is ready (useful on some boards)

  Serial.println("Connecting to Wi-Fi...");
  connectWiFi();                  // Call a function that connects to the Wi-Fi network
  Serial.println("Connected to Wi-Fi!");
}


  
void loop() {
  Serial.println("\nFetching Agriland RSS Feed...");
  getRSSData();                   // Rather than getWeatherData()
  delay(60000);                   // 1 minute delay between each time Agriland fetches the news.
}

// ----------------------------------------------------------
// Connects the UNO R4 WiFi board to your Wi-Fi network
// ----------------------------------------------------------
void connectWiFi() {
  // Try to connect using the SSID and password
  WiFi.begin(ssid, pass);

  // Keep waiting (and printing dots) until the connection is successful
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }

  // Once connected, print confirmation and the board’s IP address
  Serial.println("\nWi-Fi connected.");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

// ----------------------------------------------------------
// Sends a request to Agriland and prints the result
// ----------------------------------------------------------
void getRSSData() {
  // Build the URL path for the API request.
  // Example result: /data/2.5/weather?q=Sligo,IE&appid=YOURKEY&units=metric
  String path = rssPath;

  Serial.print("Requesting: ");
  Serial.println(path);

  // Send an HTTP GET request to that path
  client.get(path);

  // Get the status code (e.g. 200 = OK) and the full response text
  int statusCode = client.responseStatusCode();
  String response = client.responseBody();

  // Print out what we got back from the server
  Serial.print("Status code: ");
  Serial.println(statusCode);

  Serial.println("---- RSS XML Response ----");
  Serial.println(response);          // Shows the full XML feed
  Serial.println("------------------------");

  // Pull out the title of news feed and link to the news feed from the XML file.
  String newsTitle = extractValue(response, "<title>", "</title>");
  String newsLink  = extractValue(response, "<link>", "</link>");
  String newsDescription = extractValue(response, "<description>", "</description>");

  // Print the extracted values in a clean format
  Serial.println("\n----- Extracted Farmer News -----");
  Serial.print("Title: ");
  Serial.println(newsTitle);
  Serial.print("Link: ");
  Serial.println(newsLink);
  Serial.print("Description: ");
  Serial.println(newsDescription);

  Serial.println("----------------------------------");

}

  // ----------------------------------------------------------
// Finds and returns a value inside a JSON-like string.
//
// Example use:
//   extractValue("<title>"Farmer News</title>", "<title>", "</title>")
//   → returns "Farmer News"
// ----------------------------------------------------------
String extractValue(String data, String startTag, String endTag) {
  int start = data.indexOf(startTag);          // Find where the key starts
  if (start == -1) return "N/A";          // If not found, return "N/A"
  start += startTag.length();                  // Move past the key text
  int end = data.indexOf(endTag, start); // Find where the value ends
  if (end == -1) end = data.length();     // If no delimiter found, go to end of text
  return data.substring(start, end);      // Return just the value part
}


