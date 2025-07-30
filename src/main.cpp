#include <Arduino.h>
#include "WiFiServerHandler.h"
#include "SensorHandler.h"
#include "FirebaseHandler.h"


bool pumpActive = false;
unsigned long pumpStartTime = 0;
unsigned long lastPumpOffTime = 0;

const unsigned long pumpDuration = 5000;
const unsigned long pumpCooldown = 30000;
unsigned long lastSensorRead = 0;
const unsigned long sensorReadInterval = 1000;


unsigned long lastUpload = 0;
unsigned long lastSendTime = 0;

const unsigned long uploadInterval = 60000;  
const unsigned long sendInterval   = 5000;  

#define PUMP_RELAY 25

void setup() {
    Serial.begin(115200);
    pinMode(PUMP_RELAY, OUTPUT);
    digitalWrite(PUMP_RELAY, HIGH);

      initFirebase(
        "YOUR_API_KEY",
        "YOUR_EMAIL",
        "YOUR_PASSWORD",
        "YOUR_DATABASE_URL"
    );

    initSensors();
    setupWiFiAndServer();
}
String getUnixTimeString() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return "0";  // Or "" or any fallback you prefer if time not available
  }
  time_t unixTime = mktime(&timeinfo);
  if (unixTime == -1) {
    return "0";  // fallback in case mktime fails
  }
  return String(unixTime);
}
void loop() {
    loopWiFiAndServer();

    unsigned long currentTime = millis();

    if (currentTime - lastSensorRead >= sensorReadInterval) {
        lastSensorRead = currentTime;
        readSensors();
    }
     
    // Use g_sensorData instead of separate variables
    if (setUpComplete) {
        float avgTemp = (g_sensorData.temp_val_1 + g_sensorData.temp_val_2) / 2;
        float avgMoist = (g_sensorData.moist_percent_1 + g_sensorData.moist_percent_2) / 2;
        unsigned long currentTime = millis();
        String currentTimeStamp = getUnixTimeString(); 

        if (app.ready()) {
            if (currentTime - lastUpload >= uploadInterval) {
                lastUpload = currentTime;
                // ✅ Upload record data using struct
                uploadRecordDataToFirebase(currentTimeStamp, g_sensorData);
            }
            else if (currentTime - lastSendTime >= sendInterval) {
                Serial.println("Timestamp: " + currentTimeStamp);
                lastSendTime = currentTime;
                // ✅ Upload real-time data using struct
                uploadDataToFirebase(g_sensorData);
            }
        }
        if (!pumpActive && (currentTime - lastPumpOffTime >= pumpCooldown)) {
            if (avgTemp > 34 || avgMoist < 80) {
                digitalWrite(PUMP_RELAY, LOW);
                pumpStartTime = currentTime;
                pumpActive = true;
                Serial.println("Pump ON");
            }
        }

        if (pumpActive && (currentTime - pumpStartTime >= pumpDuration)) {
            digitalWrite(PUMP_RELAY, HIGH);
            pumpActive = false;
            lastPumpOffTime = currentTime;
            Serial.println("Pump OFF, cooldown started");
        }
    }
}
