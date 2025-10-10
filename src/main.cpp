#include <Arduino.h>
#include "WiFiServerHandler.h"
#include "SensorHandler.h"
#include "FirebaseHandler.h"
#include "Config.h"
#include "SerialDebugger.h"
#include "PumpHandler.h"
#include "SensorsData.h"
bool pumpActive = false;
unsigned long pumpStartTime = 0;
unsigned long lastPumpOffTime = 0;

unsigned long lastSensorRead = 0;
const unsigned long sensorReadInterval = 5000;

unsigned long lastUpload = 0;
unsigned long lastSendTime = 0;

const unsigned long uploadInterval = 60000;  
const unsigned long sendInterval   = 60000;

SensorData prevReading;


void setup() {

    Debug.begin(115200);
    initPump();
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
void firebaseSenderHandler (unsigned long currentTime) {
    if(prevReading.temp_val_1 == g_sensorData.temp_val_1 
        && prevReading.temp_val_2 == g_sensorData.temp_val_2
        &&prevReading.moist_percent_1 == g_sensorData.moist_percent_2
        &&prevReading.water_level == g_sensorData.water_level
        &&prevReading.tds_val == g_sensorData.tds_val
        &&prevReading.ph_val == g_sensorData.ph_val
        &&prevReading.ultra_level_percent == g_sensorData.ultra_level_percent){
            return;
    }
    String currentTimeStamp = getUnixTimeString(); 
    lastUpload = currentTime;
    uploadRecordDataToFirebase(currentTimeStamp, g_sensorData);
    prevReading = g_sensorData;
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


        firebaseLoop();
        if (app.ready() && !DEBUG_FIREBASE) {
            if (currentTime - lastUpload >= uploadInterval) {
                firebaseSenderHandler(currentTime);
            }
            //else if (currentTime - lastSendTime >= sendInterval) {
            //    //Debug.println("Calling firebase realtime");
            //    lastSendTime = currentTime;
            //    uploadDataToFirebase(g_sensorData);
            //}
        }
        if(!DEBUG_PUMP){
            if (!pumpActive && (currentTime - lastPumpOffTime >= PUMP_COOLDOWN)) {
                if (avgTemp > 30 || avgMoist < 75) {
                    setPump(1);
                    pumpStartTime = currentTime;
                    pumpActive = true;
                    // Debug.println("Pump is active");
                }
            }
            if (pumpActive && (currentTime - pumpStartTime >= PUMP_DURATION)) {
                setPump(0);
                pumpActive = false;
                lastPumpOffTime = currentTime;
                // Debug.println("Pump is inactive");
            }
            
        }
    }
    
}

