#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "config.hpp"
#include "display.hpp"
#include "scale.hpp"
#include "rotary.hpp" // Add this to access setupRotary and other rotary methods

WiFiClient espClient;
PubSubClient client(espClient);

//wifi not used for now. Maybe OTA later
const char *ssid = "ssid"; // Change this to your WiFi SSID
const char *password = "pw"; // Change this to your WiFi password
long lastReconnectAttempt = 0;


void setup() {
  Serial.begin(115200);
  setupDisplay();
  setupScale();
  setupRotary();
  Serial.println("System Initialized");
}

boolean reconnect() {
  if (client.connect("coffee-scale")) {
    // Once connected, publish an announcement...
    Serial.println("Connected to MQTT");
  }
  return client.connected();
}

void loop() {
    handleRotary();
    // Add other non-blocking tasks if needed
    delay(10); // Prevent excessive CPU usage
}
