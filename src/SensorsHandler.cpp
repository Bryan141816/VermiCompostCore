#include "SensorHandler.h"
#include <OneWire.h>
#include <DallasTemperature.h>
#include "Globals.h"
#include <Adafruit_ADS1X15.h>
#include "SerialDebugger.h"
#include <Preferences.h>   // (ensure this include is present)

Preferences preferences;
// DS18B20 setup
#define ONE_WIRE_BUS 14
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

// Pins
#define MOISTURE_SENSOR_1 32
#define MOISTURE_SENSOR_2 33
#define WATER_LEVEL 34
#define TdsSensorPin 35

// 🔊 Ultrasonic pins
#define ULTRA_TRIG_PIN     5
#define ULTRA_ECHO_PIN     18
#define ULTRA_TIMEOUT_US   30000UL  // ~10m max; prevents long blocking

Adafruit_ADS1115 ads;

#define VREF 3.3
#define SCOUNT 30
int analogBuffer[SCOUNT];
int analogBufferTemp[SCOUNT];
int analogBufferIndex = 0;
float temperature = 25;  // default for TDS compensation

// Calibration values
int valAir1 = 3018;
int valWater1 = 1710;
int valAir2 = 3018;
int valWater2 = 1710;
// float Tankempty = 10;
// float TankFull = 3;
bool setUpComplete = true;

// 🔊 Ultrasonic calibration defaults (edit to your tank)
// UPDATED: empty = 14 cm, full = 4 cm
float ULTRA_EMPTY_CM = 14.0f;  // distance at EMPTY (farther)
float ULTRA_FULL_CM  = 4.0f;   // distance at FULL (nearer)

// ✅ Define the global variable
SensorData g_sensorData = {0};
void loadOrSetDefaults() {
  preferences.begin("config", true);

  // Load or initialize integer values
  valAir1 = preferences.getInt("valAir1", 3018);

  valWater1 = preferences.getInt("valWater1", 1710);

  valAir2 = preferences.getInt("valAir2", 3018);

  valWater2 = preferences.getInt("valWater2", 1710);

  // Tankempty = preferences.getFloat("Tankempty", 10.0);
  // TankFull  = preferences.getFloat("TankFull", 3.0);

  // UPDATED: fallback defaults now 14 (empty) and 4 (full)
  ULTRA_EMPTY_CM = preferences.getFloat("ultraEmpty", 14.0f);
  ULTRA_FULL_CM  = preferences.getFloat("ultraFull",   4.0f);

  setUpComplete = preferences.getBool("setUpComplete", false);

  preferences.end();
}

void setUltrasonicCalibration(float empty_cm, float full_cm, bool save) {
  // guard against invalid input
  if (empty_cm <= full_cm) {
    // Keep previous values if bad input
    return;
  }
  ULTRA_EMPTY_CM = empty_cm;
  ULTRA_FULL_CM  = full_cm;

  if (save) {
    preferences.begin("config", false);
    preferences.putFloat("ultraEmpty", ULTRA_EMPTY_CM);
    preferences.putFloat("ultraFull",  ULTRA_FULL_CM);
    preferences.end();
  }
}

void initSensors() {
  pinMode(MOISTURE_SENSOR_1, INPUT);
  pinMode(MOISTURE_SENSOR_2, INPUT);
  pinMode(WATER_LEVEL, INPUT);
  pinMode(TdsSensorPin, INPUT);

  // 🔊 Ultrasonic pins
  pinMode(ULTRA_TRIG_PIN, OUTPUT);
  pinMode(ULTRA_ECHO_PIN, INPUT);

  // Ensure TRIG is low initially
  digitalWrite(ULTRA_TRIG_PIN, LOW);

  sensors.begin();
  ads.begin();

  // Load any saved calibration
  loadOrSetDefaults();
}

void readSensors() {
  sensors.requestTemperatures();

  g_sensorData.temp_val_1 = NAN;
  g_sensorData.temp_val_2 = NAN;

  DeviceAddress tempDevice1, tempDevice2;

  if (sensors.getAddress(tempDevice1, 0))
    g_sensorData.temp_val_1 = sensors.getTempC(tempDevice1);
  else
    g_sensorData.temp_val_1 = NAN;

  if (sensors.getAddress(tempDevice2, 1))
    g_sensorData.temp_val_2 = sensors.getTempC(tempDevice2);
  else
    g_sensorData.temp_val_2 = NAN;

  g_sensorData.moist_percent_1 = getMoistureVal(MOISTURE_SENSOR_1, valAir1, valWater1);
  g_sensorData.moist_percent_2 = getMoistureVal(MOISTURE_SENSOR_2, valAir2, valWater2);

  g_sensorData.water_level = getWaterLevel();

  // 🔊 Ultrasonic readings
  // Take several reads for stability, use median
  const int N = 5;
  float reads[N];
  for (int i = 0; i < N; i++) {
    reads[i] = readUltrasonicDistanceCM();

    delay(20);
  }
  // simple insertion sort
  for (int i = 1; i < N; i++) {
    float key = reads[i];
    int j = i - 1;
    while (j >= 0 && reads[j] > key) { reads[j + 1] = reads[j]; j--; }
    reads[j + 1] = key;
  }
  float dist_cm = reads[N / 2];  // median
  g_sensorData.ultra_distance_cm   = dist_cm;
  g_sensorData.ultra_level_percent = distanceToLevelPercent(dist_cm);

  // Optionally, fill TDS and pH values when available
  g_sensorData.tds_val = getTDSValue(); // Placeholder
  g_sensorData.ph_val  = getPHValue();  // Placeholder

  Debug.println("Sensor Readings:");
  Debug.println("Temperature 1: " + String(g_sensorData.temp_val_1));
  Debug.println("Temperature 2: " + String(g_sensorData.temp_val_2));
  Debug.println("Moisture 1: " + String(g_sensorData.moist_percent_1));
  Debug.println("Moisture: " + String(g_sensorData.moist_percent_2));
  Debug.println("Water Level: " + String(g_sensorData.water_level));
  Debug.println("US Distance:   " + String(g_sensorData.ultra_distance_cm) + " cm");
  Debug.println("TDS Value: " + String(g_sensorData.tds_val) + "ppm");
  Debug.println("PH Value: " + String(g_sensorData.ph_val));
}

int getMoistureVal(int PIN, int airVal, int waterVal){
  int rawVal = analogRead(PIN);
  // Debug.println(String(PIN) + " " + String(rawVal));
  int percent = map(rawVal, waterVal, airVal, 100, 0);
  return constrain(percent, 0, 100);
}

int getWaterLevel(){
  int water_level = analogRead(WATER_LEVEL);
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

  static unsigned long printTimepoint = millis();
  if (millis() - printTimepoint > 800U) {
    printTimepoint = millis();
    for (int i = 0; i < SCOUNT; i++) analogBufferTemp[i] = analogBuffer[i];
    float averageVoltage = getMedianNum(analogBufferTemp, SCOUNT) * VREF / 4095.0;

    float compensationCoefficient = 1.0 + 0.02 * (temperature - 25.0);
    float compensationVoltage = averageVoltage / compensationCoefficient;

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
  float voltage = ads.computeVolts(adc0);

  //Debug.println(String(voltage));

  float voltage_pH4 = 3.01;
  float voltage_pH6_86 = 2.60;
  float pH4 = 4.01;
  float pH6_86 = 6.86;

  float pH_step = (voltage_pH6_86 - voltage_pH4) / (pH6_86 - pH4);
  float pH_value = (voltage - voltage_pH4) / pH_step + pH4;

  return pH_value;
}

// ------------------ Ultrasonic implementation ------------------
float readUltrasonicDistanceCM() {
  // ensure clean trigger
  digitalWrite(ULTRA_TRIG_PIN, LOW);
  delayMicroseconds(3);
  // 10µs pulse to trigger
  digitalWrite(ULTRA_TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(ULTRA_TRIG_PIN, LOW);

  // measure echo pulse width
  unsigned long duration = pulseIn(ULTRA_ECHO_PIN, HIGH, ULTRA_TIMEOUT_US);
  if (duration == 0) {
    // timeout -> no echo; return a large number so % becomes 0
    return ULTRA_EMPTY_CM + 100.0f;
  }
  // HC-SR04: distance (cm) = duration(µs) / 58.0
  float cm = duration / 58.0f;
  return cm;
}

int distanceToLevelPercent(float cm) {
  // Map distance (empty..full) → 0..100%, then clamp
  // When cm == ULTRA_FULL_CM → 100%
  // When cm == ULTRA_EMPTY_CM → 0%
  int percent = (int) roundf(
      (ULTRA_EMPTY_CM - cm) * 100.0f / (ULTRA_EMPTY_CM - ULTRA_FULL_CM)
  );
  return constrain(percent, 0, 100);
}
