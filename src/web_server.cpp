#include "web_server.hpp"
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include "api_handler.hpp"
#include "config.hpp"

AsyncWebServer server(80);

String currentIPAddress = "192.168.4.1"; // Default AP mode IP

const char *apSSID = "ESP32C3_Config_openGBW";  // AP SSID
const char *apPassword = "12345678";           // AP Password (must be at least 8 characters)

void updateIPAddress() {
    if (WiFi.status() == WL_CONNECTED) {
        currentIPAddress = WiFi.localIP().toString();
    } else {
        currentIPAddress = "192.168.4.1";
    }
}

void connectToWiFi() {
    preferences.begin("wifi", true);  // Open preferences in read mode
    String storedSSID = preferences.getString("wifi_ssid", "");
    String storedPass = preferences.getString("wifi_pass", "");
    preferences.end();  // Close preferences

    if (storedSSID.length() > 0) {
        Serial.print("Connecting to WiFi: ");
        Serial.println(storedSSID);

        WiFi.begin(storedSSID.c_str(), storedPass.c_str());
        unsigned long startAttemptTime = millis();

        while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 10000) {
            delay(500);
            Serial.print(".");
        }

        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("\nWiFi Connected!");
            Serial.print("IP Address: ");
            Serial.println(WiFi.localIP());
            updateIPAddress();
            return;
        }
    }

    Serial.println("Failed to connect. Starting AP mode...");
    WiFi.mode(WIFI_AP);
    WiFi.softAP(apSSID, apPassword);
    delay(500); // Ensure the AP starts properly
    updateIPAddress();
    Serial.print("AP IP Address: ");
    Serial.println(WiFi.softAPIP());
}

void handleWiFiConfig(AsyncWebServerRequest *request) {
    if (request->hasParam("ssid") && request->hasParam("pass")) {
        String newSSID = request->getParam("ssid")->value();
        String newPass = request->getParam("pass")->value();

        preferences.begin("wifi", false);
        preferences.putString("wifi_ssid", newSSID);
        preferences.putString("wifi_pass", newPass);
        preferences.end(); // Close preferences

        Serial.println("WiFi credentials SAVED. Rebooting in 5 seconds...");
        request->send(200, "text/plain", "WiFi credentials SAVED. Rebooting...");
        delay(5000);
        ESP.restart();
    } else {
        Serial.println("Missing SSID or Password");
        request->send(400, "text/plain", "Missing SSID or Password");
    }
}

void setupWebServer() {
    Serial.println("Starting Web Server...");
    connectToWiFi();

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/html",
            "<html><head><meta name='viewport' content='width=device-width, initial-scale=1'>"
            "<style>"
            "body { font-family: Arial, sans-serif; text-align: center; background-color: #1c1c1c; color: white; }"
            ".container { max-width: 400px; margin: auto; background: #2a2a2a; padding: 20px; border-radius: 10px; }"
            "input, button { width: 100%; padding: 10px; margin-top: 10px; border-radius: 5px; border: none; }"
            "input { background: #333; color: white; }"
            "button { background: #28a745; color: white; cursor: pointer; }"
            "button:hover { background: #218838; }"
            "</style></head><body>"
            "<div class='container'><h2>Wi-Fi Setup</h2>"
            "<form action='/save' method='GET'>"
            "<label>SSID:</label><input name='ssid'><br>"
            "<label>Password:</label><input name='pass' type='password'><br>"
            "<button type='submit'>Save</button>"
            "</form></div></body></html>");
    });

    server.on("/resetWifi", HTTP_GET, [](AsyncWebServerRequest *request) {
        Serial.println("Resetting WiFi credentials...");
        preferences.begin("wifi", false);
        preferences.remove("wifi_ssid");
        preferences.remove("wifi_pass");
        preferences.end();
        
        Serial.println("WiFi credentials erased. Rebooting in 5 seconds...");
        request->send(200, "text/plain", "WiFi credentials ERASED. Rebooting...");        
        delay(5000);
        ESP.restart();
    });

    server.on("/save", HTTP_GET, handleWiFiConfig);

    server.begin();
    Serial.println("Web Server Started.");
}
