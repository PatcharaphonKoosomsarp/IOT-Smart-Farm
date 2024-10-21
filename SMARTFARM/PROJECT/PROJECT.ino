#include <Wire.h>
#include "FirebaseESP8266.h"
#include <ESP8266WiFi.h>
#include "DHT.h"
#include <time.h>  // For NTP time
#include <BH1750.h>
#include <TridentTD_LineNotify.h>  // ไลบรารี LINE Notify

// Pin and constants
#define DHTPIN D4      // Pin connected to the DHT22 data pin
#define DHTTYPE DHT22  // DHT 22 (AM2302)

// Firebase credentials
char FIREBASE_AUTH[] = "xFIACbUpoxCtYuDWPoWbRh9NxCqtOvkSF7aIYuKP";
char FIREBASE_HOST[] = "https://smartfarm-b1cd9-default-rtdb.asia-southeast1.firebasedatabase.app";

// WiFi credentials
char WIFI_SSID[] = "B415";
char WIFI_PASSWORD[] = "appletv415";

// LINE Notify token
#define LINE_TOKEN "oupAr0Okmz6uJiJ0oBEKi0bhABqddSpMlsKsqOfK40e"

const int RED_PIN = D5;     // Fan control (FAN)
const int GREEN_PIN = D6;   // Light control (LIGHT)
const int YELLOW_PIN = D7;  // Water pump control (PUMP)

// Thresholds
const float TEMP_THRESHOLD = 30.0;   // Temperature threshold
const float LIGHT_THRESHOLD = 10.0;  // Low light threshold

FirebaseData firebaseData;
DHT dht(DHTPIN, DHTTYPE);  // Initialize DHT sensor
BH1750 lightMeter;

bool lastFanState = false;
bool lastLightState = false;
bool lastPumpState = false;

void setup() {
  Serial.begin(115200);
  Wire.begin();
  lightMeter.begin();
  pinMode(RED_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(YELLOW_PIN, OUTPUT);

  // Start WiFi connection
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWi-Fi connected");

  // Start Firebase connection
  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);

  // Start DHT sensor
  dht.begin();

  // Start NTP to get the time (UTC+7 for Thailand)
  configTime(7 * 3600, 0, "pool.ntp.org", "time.nist.gov");
  Serial.println("\nWaiting for NTP time sync...");
  while (!time(nullptr)) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("\nTime synced!");

  // Initialize LINE Notify
  LINE.setToken(LINE_TOKEN);
}

String getFormattedTime() {
  time_t now = time(nullptr);
  struct tm* timeinfo = localtime(&now);
  char buffer[20];
  strftime(buffer, sizeof(buffer), "%Y/%m/%d %H:%M:%S", timeinfo);
  return String(buffer);
}

void sendLineNotify(float temperature, float humidity, float light, bool fanOn, bool lightOn, bool pumpOn, String time) {
  String message = "\nTemperature: " + String(temperature) + "°C\n" + "Humidity: " + String(humidity) + "%\n" + "Light: " + String(light) + " lx\n" + "FAN: " + (fanOn ? "ON" : "OFF") + "\n" + "LIGHT: " + (lightOn ? "ON" : "OFF") + "\n" + "PUMP: " + (pumpOn ? "ON" : "OFF") + "\n" + "Date: " + time.substring(0, 10) + "\n" + "Time: " + time.substring(11);
  LINE.notify(message);
}

void controlFan(float temperature) {
  bool fanOn = (temperature > TEMP_THRESHOLD);
  digitalWrite(RED_PIN, fanOn ? HIGH : LOW);  // Control Fan (RED_PIN)
  if (fanOn != lastFanState) {
    lastFanState = fanOn;
    
  }
  Serial.println("Fan state changed: " + String(fanOn ? "ON" : "OFF"));
}

void controlLight(float lux) {
  bool lightOn = (lux < LIGHT_THRESHOLD);
  digitalWrite(GREEN_PIN, lightOn ? HIGH : LOW);  // Control Light (GREEN_PIN)
  if (lightOn != lastLightState) {
    lastLightState = lightOn;
    
  }
  Serial.println("Light state changed: " + String(lightOn ? "ON" : "OFF"));
}

void controlPump(bool turnOn) {
  digitalWrite(YELLOW_PIN, turnOn ? HIGH : LOW);  // Control Pump (YELLOW_PIN)
  if (turnOn != lastPumpState) {
    lastPumpState = turnOn;
    
  }
  Serial.println("Pump state changed: " + String(turnOn ? "ON" : "OFF"));
}

void loop() {
  // Get sensor data
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();
  float lux = lightMeter.readLightLevel();
  String timestamp = getFormattedTime();

  // Check if the data from the sensors is valid
  if (isnan(temperature) || isnan(humidity) || lux < 0) {
    Serial.println("Failed to read from sensors");
    return;
  }

  // Log sensor data to Serial Monitor
  Serial.println("------------------ Value -------------------");
  Serial.println("Timestamp: " + timestamp);
  Serial.println("Temperature: " + String(temperature) + " °C");
  Serial.println("Humidity: " + String(humidity) + " %");
  Serial.println("Light Intensity: " + String(lux) + " lx");
  Serial.println("------------------ Status -------------------");

    // Firebase log path
    String logPath = "/logs/log_" + String(millis());

  // Send data to Firebase
  Firebase.setFloat(firebaseData, logPath + "/temperature", temperature);
  Firebase.setFloat(firebaseData, logPath + "/humidity", humidity);
  Firebase.setFloat(firebaseData, logPath + "/light", lux);
  Firebase.setString(firebaseData, logPath + "/time", timestamp);

  // Add states to Firebase
  Firebase.setString(firebaseData, logPath + "/led", lastLightState ? "ON" : "OFF");  // GREEN_PIN state (LED)
  Firebase.setString(firebaseData, logPath + "/fan", lastFanState ? "ON" : "OFF");    // RED_PIN state (Fan)
  Firebase.setString(firebaseData, logPath + "/pump", lastPumpState ? "ON" : "OFF");  // YELLOW_PIN state (Pump)

  // Control devices based on sensor data
  controlFan(temperature);
  controlLight(lux);

  // Pump is controlled by humidity
  controlPump(humidity > 50.0);

  // Send LINE Notify if any state changes
  if (lastFanState || lastLightState || lastPumpState) {
    sendLineNotify(temperature, humidity, lux, lastFanState, lastLightState, lastPumpState, timestamp);
  }

  // Wait for 30 seconds before the next loop
  delay(30000);
}
