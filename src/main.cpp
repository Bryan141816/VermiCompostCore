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

// Vermi Tea level thresholds (ultrasonic percent full)
const int VERMITEA_BLOCK_ON_THRESHOLD = 90;  // ≥90% → never turn ON
const int VERMITEA_RESUME_THRESHOLD   = 85;  // <85% → allow ON again (hysteresis)

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
    return "0";
  }
  time_t unixTime = mktime(&timeinfo);
  if (unixTime == -1) {
    return "0";
  }
  return String(unixTime);
}

void firebaseSenderHandler(unsigned long currentTime) {
  // Correct, field-by-field equality check
  if (prevReading.temp_val_1          == g_sensorData.temp_val_1 &&
      prevReading.temp_val_2          == g_sensorData.temp_val_2 &&
      prevReading.moist_percent_1     == g_sensorData.moist_percent_1 &&
      prevReading.moist_percent_2     == g_sensorData.moist_percent_2 &&
      prevReading.water_level         == g_sensorData.water_level &&
      prevReading.tds_val             == g_sensorData.tds_val &&
      prevReading.ph_val              == g_sensorData.ph_val &&
      prevReading.ultra_level_percent == g_sensorData.ultra_level_percent) {
    return; // no change → skip upload
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

  if (setUpComplete) {
    // Average temperature (simple mean; adjust if you want to ignore NaNs)
    float avgTemp = (g_sensorData.temp_val_1 + g_sensorData.temp_val_2) / 2.0f;

    // Average moisture from both probes
    const int avgMoist = getAvgMoisture();

    // Ultrasonic (% full) used as Vermi Tea tank level
    const int vermiTeaLevel = g_sensorData.ultra_level_percent;

    firebaseLoop();

    if (app.ready() && !DEBUG_FIREBASE) {
      if (currentTime - lastUpload >= uploadInterval) {
        firebaseSenderHandler(currentTime);
      }
      // else if (currentTime - lastSendTime >= sendInterval) {
      //   lastSendTime = currentTime;
      //   uploadDataToFirebase(g_sensorData);
      // }
    }

    if (!DEBUG_PUMP) {
      // 1) Safety: if tank water level sensor says empty/invalid, force OFF
      if (g_sensorData.water_level <= 0) {
        if (pumpActive) {
          setPump(0);
          pumpActive = false;
          lastPumpOffTime = currentTime;
          // Debug.println("⚠ Pump OFF: Water level too low");
        }
        return; // skip rest of pump logic
      }

      // 2) Hard block: if Vermi Tea ≥ 90%, ensure pump is OFF and block ON
      if (vermiTeaLevel >= VERMITEA_BLOCK_ON_THRESHOLD) {
        if (pumpActive) {
          setPump(0);
          pumpActive = false;
          lastPumpOffTime = currentTime;
          // Debug.println("⚠ Pump OFF: Vermi Tea ≥ 90%");
        }
        return; // do not allow turn ON while ≥90%
      }

      // 3) Optional hysteresis: only allow ON again when level < 85%
      if (!pumpActive && vermiTeaLevel > VERMITEA_RESUME_THRESHOLD) {
        // Still too high to turn ON
        return;
      }

      // 4) Normal pump control logic
      // Turn ON if cooldown elapsed AND (avg moisture low OR temp high)
      if (!pumpActive && (currentTime - lastPumpOffTime >= PUMP_COOLDOWN)) {
        if (avgMoist < 60 || avgTemp > 30.0f) {
          setPump(1);
          pumpStartTime = currentTime;
          pumpActive = true;
          // Debug.println("Pump ON: Avg moisture low or temp high");
        }
      }

      // Turn OFF if duration elapsed OR avg moisture restored
      if (pumpActive &&
          ((currentTime - pumpStartTime >= PUMP_DURATION) || (avgMoist > 80))) {
        setPump(0);
        pumpActive = false;
        lastPumpOffTime = currentTime;
        // Debug.println("Pump OFF: Avg moisture restored or duration ended");
      }
    }
  }
}
