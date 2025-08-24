#include "SensorHandler.h"
#include <OneWire.h>
#include <DallasTemperature.h>
#include "Globals.h"
#include <Adafruit_ADS1X15.h>
#include "SerialDebugger.h"

Preferences preferences;

// =================== DS18B20 setup ===================
#define ONE_WIRE_BUS 14
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

// =================== Pin map ===================
#define MOISTURE_SENSOR_1 32
#define MOISTURE_SENSOR_2 33
#define WATER_LEVEL       34
#define TdsSensorPin      35

// ---- Ultrasonic (HC-SR04) ----
#define ULTRA_TRIG        5     // use voltage divider on ECHO!
#define ULTRA_ECHO        18

Adafruit_ADS1115 ads;

#define VREF 3.3
#define SCOUNT 30
int   analogBuffer[SCOUNT];
int   analogBufferTemp[SCOUNT];
int   analogBufferIndex = 0;
float temperature = 25;  // default for TDS compensation

// Calibration values
int  valAir1 = 3018;
int  valWater1 = 1710;
int  valAir2 = 3018;
int  valWater2 = 1710;
bool setUpComplete = true;

SensorData g_sensorData = {0};

// ---------- forward decls (ultrasonic helpers) ----------
static float readUltrasonicCM(float tempC);
static float clampTemp(float t);

// =================== config load ===================
void loadOrSetDefaults() {
  preferences.begin("config", true);

  valAir1       = preferences.getInt("valAir1", 3018);
  valWater1     = preferences.getInt("valWater1", 1710);
  valAir2       = preferences.getInt("valAir2", 3018);
  valWater2     = preferences.getInt("valWater2", 1710);
  setUpComplete = preferences.getBool("setUpComplete", false);

  preferences.end();
}

// =================== init ===================
void initSensors() {
  pinMode(MOISTURE_SENSOR_1, INPUT);
  pinMode(MOISTURE_SENSOR_2, INPUT);
  pinMode(WATER_LEVEL,       INPUT);
  pinMode(TdsSensorPin,      INPUT);

  // Ultrasonic pins
  pinMode(ULTRA_TRIG, OUTPUT);
  pinMode(ULTRA_ECHO, INPUT);
  digitalWrite(ULTRA_TRIG, LOW);

  sensors.begin();
  ads.begin();
}

// =================== utils ===================
void printAddress(DeviceAddress deviceAddress) {
  for (uint8_t i = 0; i < 8; i++) {
    if (deviceAddress[i] < 16) Debug.print("0");
    Debug.print(String(deviceAddress[i]));
  }
  Debug.println("");
}

// =================== main read ===================
void readSensors() {
  // 🔍 Scan and print all OneWire devices
  Debug.println("Scanning for OneWire devices...");
  DeviceAddress deviceAddress;
  int deviceCount = 0;
  while (sensors.getAddress(deviceAddress, deviceCount)) {
    Debug.print("Device ");
    Debug.print(String(deviceCount));
    Debug.print(": ");
    printAddress(deviceAddress);
    deviceCount++;
  }
  Debug.print("Total OneWire devices found: ");
  Debug.println(String(deviceCount));

  // ---- Temperature(s) ----
  sensors.requestTemperatures();

  g_sensorData.temp_val_1 = NAN;
  g_sensorData.temp_val_2 = NAN;

  DeviceAddress tempDevice1, tempDevice2;

  if (sensors.getAddress(tempDevice1, 0))
    g_sensorData.temp_val_1 = sensors.getTempC(tempDevice1);

  if (sensors.getAddress(tempDevice2, 1))
    g_sensorData.temp_val_2 = sensors.getTempC(tempDevice2);

  // ---- Moisture, water level, TDS, pH ----
  g_sensorData.moist_percent_1 = getMoistureVal(MOISTURE_SENSOR_1, valAir1, valWater1);
  g_sensorData.moist_percent_2 = getMoistureVal(MOISTURE_SENSOR_2, valAir2, valWater2);
  g_sensorData.water_level     = getWaterLevel();
  g_sensorData.tds_val         = getTDSValue();
  g_sensorData.ph_val          = getPHValue();

  // ---- Ultrasonic distance (cm) with temp compensation) ----
  // pick a reasonable temp for compensation (prefer the first DS18B20 if valid)
  float tempForSonic = isfinite(g_sensorData.temp_val_1) ? g_sensorData.temp_val_1 : 25.0f;
  float distanceCM   = readUltrasonicCM(tempForSonic);

  // (Optional) store in struct if you add a field:
  // g_sensorData.ultra_distance_cm = distanceCM;

  // ---- Logs ----
  Debug.println("Sensor Readings:");
  Debug.println("Temperature 1: " + String(g_sensorData.temp_val_1));
  Debug.println("Temperature 2: " + String(g_sensorData.temp_val_2));
  Debug.println("Moisture 1: "    + String(g_sensorData.moist_percent_1));
  Debug.println("Moisture 2: "    + String(g_sensorData.moist_percent_2));
  Debug.println("Water Level: "   + String(g_sensorData.water_level));
  Debug.println("TDS Value: "     + String(g_sensorData.tds_val) + " ppm");
  Debug.println("PH Value: "      + String(g_sensorData.ph_val));
  if (distanceCM < 0) {
    Debug.println("Ultrasonic: Error (no echo)");
  } else {
    Debug.println("Ultrasonic Distance: " + String(distanceCM, 1) + " cm");
  }
}

// =================== helpers ===================
int getMoistureVal(int PIN, int airVal, int waterVal) {
  int rawVal = analogRead(PIN);
  Debug.println(String(PIN) + " " + String(rawVal));
  int percent = map(rawVal, waterVal, airVal, 100, 0);
  return constrain(percent, 0, 100);
}

int getWaterLevel() {
  int water_level   = analogRead(WATER_LEVEL);
  int water_percent = map(water_level, 0, 2460, 0, 100);
  return constrain(water_percent, 0, 100);
}

float getTDSValue() {
  static unsigned long analogSampleTimepoint = millis();
  if (millis() - analogSampleTimepoint > 40U) {
    analogSampleTimepoint = millis();
    analogBuffer[analogBufferIndex] = analogRead(TdsSensorPin);
    analogBufferIndex++;
    if (analogBufferIndex == SCOUNT) analogBufferIndex = 0;
  }

  static unsigned long computeTimepoint = millis();
  if (millis() - computeTimepoint > 800U) {
    computeTimepoint = millis();
    for (int i = 0; i < SCOUNT; i++) analogBufferTemp[i] = analogBuffer[i];
    float averageVoltage = getMedianNum(analogBufferTemp, SCOUNT) * VREF / 4095.0;

    float compensationCoefficient = 1.0 + 0.02 * (temperature - 25.0);
    float compensationVoltage     = averageVoltage / compensationCoefficient;

    float tdsValue = (133.42 * compensationVoltage * compensationVoltage * compensationVoltage
                    - 255.86 * compensationVoltage * compensationVoltage
                    + 857.39 * compensationVoltage) * 0.5;
    return tdsValue;
  }
  return 0;
}

int getMedianNum(int bArray[], int len) {
  int sorted[len];
  memcpy(sorted, bArray, len * sizeof(int));
  for (int i = 0; i < len - 1; i++) {
    for (int j = 0; j < len - i - 1; j++) {
      if (sorted[j] > sorted[j + 1]) {
        int temp = sorted[j];
        sorted[j] = sorted[j + 1];
        sorted[j + 1] = temp;
      }
    }
  }
  return len % 2 ? sorted[len / 2] : (sorted[len / 2 - 1] + sorted[len / 2]) / 2;
}

float getPHValue() {
  int16_t adc0 = ads.readADC_SingleEnded(0);
  float   voltage = ads.computeVolts(adc0);

  // your current 2‑point calibration
  float voltage_pH4     = 3.01;
  float voltage_pH6_86  = 2.60;
  float pH4             = 4.01;
  float pH6_86          = 6.86;

  float pH_step  = (voltage_pH6_86 - voltage_pH4) / (pH6_86 - pH4);
  float pH_value = (voltage - voltage_pH4) / pH_step + pH4;

  return pH_value;
}

// =================== Ultrasonic implementation ===================
static float readUltrasonicCM(float tempC) {
  tempC = clampTemp(tempC);

  // Speed of sound (m/s) ≈ 331.4 + 0.6 * T(°C)
  // Convert to cm/µs: (m/s * 100) / 1,000,000
  float speed_cm_per_us = (331.4f + 0.6f * tempC) * 0.0001f;

  // 10 µs trigger pulse
  digitalWrite(ULTRA_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(ULTRA_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(ULTRA_TRIG, LOW);

  // Echo duration (µs), timeout ~30 ms (≈ 5 m)
  unsigned long duration = pulseIn(ULTRA_ECHO, HIGH, 30000UL);
  if (duration == 0) return -1.0f; // timeout

  // one-way distance (cm)
  float distance_cm = (duration * speed_cm_per_us) / 2.0f;
  return distance_cm;
}

static float clampTemp(float t) {
  if (t < -20.0f) return -20.0f;
  if (t > 60.0f)  return 60.0f;
  return t;
}
