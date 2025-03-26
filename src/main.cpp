#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ESPAsyncWebServer.h>

#include "display.hpp"
#include "scale.hpp"
#include "config.hpp"
#include "web_server.hpp"

// Definitions of global variables (memory allocated here)
Preferences preferences;             // Preferences object
HX711 loadcell;                      // HX711 load cell object
SimpleKalmanFilter kalmanFilter(0.05, 0.05, 0.01); // Kalman filter for weight smoothing

TaskHandle_t ScaleTask = nullptr;    // Initialize task handles to nullptr
TaskHandle_t ScaleStatusTask = nullptr;

volatile bool displayLock = false; 

void setup() {
    Serial.begin(115200);
    
    // Setup other components
    setupDisplay();
    setupScale();
    setupWebServer();
}

void loop() {
    delay(1000);
}
