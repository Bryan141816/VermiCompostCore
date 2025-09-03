#include <Arduino.h>
#include "WiFiServerHandler.h"
#include "SensorHandler.h"
#include "FirebaseHandler.h"
#include "Config.h"
#include "SerialDebugger.h"

bool pumpActive = false;
unsigned long pumpStartTime = 0;
unsigned long lastPumpOffTime = 0;

unsigned long lastSensorRead = 0;
const unsigned long sensorReadInterval = 1000;

unsigned long lastUpload = 0;
unsigned long lastSendTime = 0;

const unsigned long uploadInterval = 6000;  
const unsigned long sendInterval   = 5000;

#define PUMP_RELAY 25

void setup() {

    Debug.begin(115200);

    if(!DEBUG_PUMP){
        pinMode(PUMP_RELAY, OUTPUT);
        digitalWrite(PUMP_RELAY, HIGH);
    }
    #if !DEBUG_WIFI_SERVER
        setupWiFiAndServer();
    #endif
    #if !DEBUG_FIREBASE
        initFirebase(FIREBASE_API_KEY, EMAIL, PASSWORD, FIREBASE_DB_URL);
    #endif

    initSensors();

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
    if (true) {
        float avgTemp = (g_sensorData.temp_val_1 + g_sensorData.temp_val_2) / 2;
        float avgMoist = (g_sensorData.moist_percent_1 + g_sensorData.moist_percent_2) / 2;
        unsigned long currentTime = millis();
        String currentTimeStamp = getUnixTimeString(); 
        Debug.println(String(currentTime) + "Ms");
        Debug.println(String(lastSendTime)+"ms");
        firebaseLoop();
        if (app.ready()) {
            if (currentTime - lastUpload >= uploadInterval) {
                Debug.println("Calling firebase");
                lastUpload = currentTime;
                uploadRecordDataToFirebase(currentTimeStamp, g_sensorData);
            }
            else if (currentTime - lastSendTime >= sendInterval) {
                Debug.println("Calling firebase realtime");
                lastSendTime = currentTime;
                uploadDataToFirebase(g_sensorData);
            }
        }
        if(!DEBUG_PUMP){
            if (!pumpActive && (currentTime - lastPumpOffTime >= PUMP_COOLDOWN)) {
                if (avgTemp > 34 || avgMoist < 80) {
                    digitalWrite(PUMP_RELAY, HIGH);
                    pumpStartTime = currentTime;
                    pumpActive = true;
                    Debug.println("Pump is active");
                }
            }
            if (pumpActive && (currentTime - pumpStartTime >= PUMP_DURATION)) {
                digitalWrite(PUMP_RELAY, LOW);
                pumpActive = false;
                lastPumpOffTime = currentTime;
                Debug.println("Pump is inactive");
            }
        }
    }
}
