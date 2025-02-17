#include "web_server.hpp"
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <config.hpp>
#include <Preferences.h>

AsyncWebServer server(80);

String currentIPAddress = "192.168.4.1"; // Default for AP mode

const char *apSSID = "ESP32C3_Config_openGBW";  // AP SSID
const char *apPassword = "12345678";  // AP Password (must be at least 8 chars)

void updateIPAddress() {
    if (WiFi.status() == WL_CONNECTED) {
        currentIPAddress = WiFi.localIP().toString();
    } else {
        currentIPAddress = "192.168.4.1";
    }
}

void connectToWiFi() {
    String storedSSID = preferences.getString("wifi_ssid", "");
    String storedPass = preferences.getString("wifi_pass", "");

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
            return;
        }
    }

    Serial.println("Failed to connect. Starting AP mode...");
    WiFi.mode(WIFI_AP);
    WiFi.softAP(apSSID, apPassword);
    Serial.print("AP IP Address: ");
    Serial.println(WiFi.softAPIP());
}

void handleWiFiConfig(AsyncWebServerRequest *request) {
    if (request->hasParam("ssid") && request->hasParam("pass")) {
        String newSSID = request->getParam("ssid")->value();
        String newPass = request->getParam("pass")->value();

        preferences.putString("wifi_ssid", newSSID);
        preferences.putString("wifi_pass", newPass);

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
    preferences.begin("wifi", false);
    connectToWiFi();

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/html",
            "<form method='GET' action='/save'>"
            "<label>SSID:</label><input name='ssid'><br>"
            "<label>Password:</label><input name='pass' type='password'><br>"
            "<button type='submit'>Save</button>"
            "</form>");
    });

    server.on("/resetWifi", HTTP_GET, [](AsyncWebServerRequest *request) {
        Serial.println("Resetting WiFi credentials...");
        preferences.remove("wifi_ssid");
        preferences.remove("wifi_pass");
        Serial.println("WiFi credentials erased. Rebooting in 5 seconds...");
        request->send(200, "text/plain", "WiFi credentials ERASED. Rebooting...");        
        delay(5000);  // Ensure the message is sent before rebooting
        ESP.restart();
    });
    

    server.on("/save", HTTP_GET, handleWiFiConfig);
    server.begin();
}
