#include "WiFiServerHandler.h"
#include <ArduinoOTA.h>
#include <ESPmDNS.h>
#include "SensorsData.h"  // ✅ Include global sensor struct
#include "SensorHandler.h"
#include "Globals.h"

WebServer server(80);
WiFiServer telnetServer(23);
WiFiClient telnetClient;

const char* ap_ssid = "Vermi_Compost_1934";
const char* mdnshost = "vermi1934";
const char* DEVICE_ID = "1934";

IPAddress local_IP(10, 0, 0, 1);
IPAddress gateway(10, 0, 0, 1);
IPAddress subnet(255, 255, 255, 0);

void handlePing() {
    server.send(200, "text/plain", "");
}

void handleHandshake() {
    String json = "{\"device_id\": \"" + String(DEVICE_ID) +
                  "\", \"device_mdns\": \"" + String(mdnshost) +
                  "\", \"device_name\": \"" + String(ap_ssid) + "\"}";
    server.send(200, "application/json", json);
}

void handleConnectToNetwork() {
    if (!server.hasArg("ssid") || !server.hasArg("password")) {
        server.send(400, "text/plain", "Missing ssid or password");
        return;
    }

    String ssid = server.arg("ssid");
    String password = server.arg("password");

    Serial.printf("Connecting to WiFi SSID: %s\n", ssid.c_str());
    WiFi.mode(WIFI_AP_STA);
    WiFi.enableAP(true);
    WiFi.begin(ssid.c_str(), password.c_str());

    int retries = 0;
    while (WiFi.status() != WL_CONNECTED && retries < 20) {
        delay(500);
        Serial.print(".");
        retries++;
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("WiFi connected successfully.");
        preferences.begin("wifi", false);
        preferences.putString("ssid", ssid);
        preferences.putString("password", password);
        preferences.end();
        server.send(200, "text/plain", "success");
    } else {
        Serial.println("WiFi connection failed.");
        server.send(200, "text/plain", "failed");
    }
}

void handleConfirm() {
    Serial.println("Disabling AP mode...");
    WiFi.softAPdisconnect(true);
    server.send(200, "text/plain", "AP disabled");
}

void handleResetWifi() {
    preferences.begin("wifi", false);
    preferences.clear();
    preferences.end();

    preferences.begin("config", false);
    preferences.clear();
    preferences.end();

    Serial.println("WiFi credentials cleared.");
    WiFi.disconnect(true);
    delay(1000);

    WiFi.mode(WIFI_AP);
    WiFi.softAP("Vermi_Compost_Reset");
    Serial.println("AP restarted after reset.");
    server.send(200, "text/plain", "WiFi credentials reset and AP re-enabled");
    ESP.restart();
}

void handleGetData() {
    String json = "{";
    json += "\"temp0\":" + String(g_sensorData.temp_val_1, 2) + ",";
    json += "\"temp1\":" + String(g_sensorData.temp_val_2, 2) + ",";
    json += "\"moisture1\":" + String(g_sensorData.moist_percent_1) + ",";
    json += "\"moisture2\":" + String(g_sensorData.moist_percent_2) + ",";
    json += "\"water_level\":" + String(g_sensorData.water_level);
    json += "}";
    server.send(200, "application/json", json);
}

void handleCalibration() {
    if (!server.hasArg("target")) {
        server.send(400, "text/plain", "Missing target or value");
        return;
    }

    String target = server.arg("target");
    preferences.begin("config", false);

    if (target == "moisture_dry") {
        preferences.putInt("valAir1", g_sensorData.moist_percent_1);
        preferences.putInt("valAir2", g_sensorData.moist_percent_2);
        valAir1 = g_sensorData.moist_percent_1;
        valAir2 = g_sensorData.moist_percent_2;
        server.send(200, "text/plain", "Moisture dry calibrated.");
    } else if (target == "moisture_wet") {
        preferences.putInt("valWater1", g_sensorData.moist_percent_1);
        preferences.putInt("valWater2", g_sensorData.moist_percent_2);
        valWater1 = g_sensorData.moist_percent_1;
        valWater2 = g_sensorData.moist_percent_2;
        server.send(200, "text/plain", "Moisture wet calibrated.");
    } 
    // else if (target == "tankempty") {
    //     preferences.putFloat("Tankempty", g_sensorData.water_level);
    //     Tankempty = g_sensorData.water_level;
    //     server.send(200, "text/plain", "Tank empty calibrated.");
    // } else if (target == "tankfull") {
    //     preferences.putFloat("TankFull", g_sensorData.water_level);
    //     if (!setUpComplete) {
    //         preferences.putBool("setUpComplete", true);
    //         setUpComplete = true;
    //     }
    //     TankFull = g_sensorData.water_level;
    //     server.send(200, "text/plain", "Tank full calibrated.");
    // } 
    else {
        server.send(400, "text/plain", "Unknown target.");
    }

    preferences.end();
}

void connectWithSavedCredentials() {
    preferences.begin("wifi", true);
    String saved_ssid = preferences.getString("ssid", "");
    String saved_password = preferences.getString("password", "");
    preferences.end();

    if (saved_ssid.length() > 0) {
        WiFi.mode(WIFI_STA);
        WiFi.begin(saved_ssid.c_str(), saved_password.c_str());

        int retries = 0;
        while (WiFi.status() != WL_CONNECTED && retries < 20) {
            delay(500);
            Serial.print(".");
            retries++;
        }
        Serial.println();

        if (WiFi.status() == WL_CONNECTED) {
            ArduinoOTA.setPassword("VermiDev1929");
            ArduinoOTA.begin();
            telnetServer.begin();
            telnetServer.setNoDelay(true);
            return;
        }
    }

    // Fallback to AP mode
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(local_IP, gateway, subnet);
    WiFi.softAP(ap_ssid);
}

void setupWiFiAndServer() {
    connectWithSavedCredentials();

    if (WiFi.status() == WL_CONNECTED) {
        configTime(8 * 3600, 0, "pool.ntp.org", "time.nist.gov");
    }

    server.on("/handshake", HTTP_GET, handleHandshake);
    server.on("/connect_to_network", HTTP_GET, handleConnectToNetwork);
    server.on("/confirm", HTTP_GET, handleConfirm);
    server.on("/calibrate", HTTP_GET, handleCalibration);
    server.on("/reset_wifi", HTTP_GET, handleResetWifi);
    server.on("/get_data", HTTP_GET, handleGetData);
    server.on("/ping", HTTP_GET, handlePing);
    server.begin();

    if (!MDNS.begin(mdnshost)) {
        Serial.println("Error starting mDNS");
    } else {
        Serial.println("mDNS responder started: http://" + String(mdnshost) + ".local");
    }
}

void loopWiFiAndServer() {
    if (WiFi.status() == WL_CONNECTED) {
        ArduinoOTA.handle();

        if (telnetServer.hasClient()) {
            if (!telnetClient || !telnetClient.connected()) {
                if (telnetClient) telnetClient.stop();
                telnetClient = telnetServer.available();
                Serial.println("New Telnet client");
            } else {
                WiFiClient newClient = telnetServer.available();
                newClient.println("Only one client allowed");
                newClient.stop();
            }
        }
    }
    server.handleClient();
}
