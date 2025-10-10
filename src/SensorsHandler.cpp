#include "SensorHandler.h"
#include <OneWire.h>
#include <DallasTemperature.h>
#include "Globals.h"
#include <Adafruit_ADS1X15.h>
#include "SerialDebugger.h"

Preferences preferences;
// DS18B20 setup
#define ONE_WIRE_BUS 14
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
int distanceToLevelPercent(float cm);
float readUltrasonicDistanceCM();

// Pins
#define MOISTURE_SENSOR_1 32
#define MOISTURE_SENSOR_2 33
#define WATER_LEVEL       34
#define TdsSensorPin      35

Adafruit_ADS1115 ads;

#define VREF   3.3
#define SCOUNT 30
int   analogBuffer[SCOUNT];
int   analogBufferTemp[SCOUNT];
int   analogBufferIndex = 0;
float temperature = 25;  // default for TDS compensation

// Calibration values
int  valAir1   = 3018;
int  valWater1 = 1710;
int  valAir2   = 3018;
int  valWater2 = 1710;
// float Tankempty = 10;
// float TankFull  = 3;
bool setUpComplete = true;

// ✅ Define the global variable
SensorData g_sensorData = {0};

// 🔊 Ultrasonic pins
#define ULTRA_TRIG_PIN     5
#define ULTRA_ECHO_PIN     18
#define ULTRA_TIMEOUT_US   30000UL  // ~5 m max window

// Calibrate these to your actual tank distances
float ULTRA_EMPTY_CM = 14.0f;  // distance when tank is EMPTY (farther)
float ULTRA_FULL_CM  = 4.0f;   // distance when tank is FULL  (nearer)

void loadOrSetDefaults() {
  preferences.begin("config", true);

  // Load or initialize integer values
  valAir1   = preferences.getInt("valAir1",   3018);
  valWater1 = preferences.getInt("valWater1", 1710);
  valAir2   = preferences.getInt("valAir2",   3018);
  valWater2 = preferences.getInt("valWater2", 1710);

  // Tankempty = preferences.getFloat("Tankempty", 10.0);
  // TankFull  = preferences.getFloat("TankFull", 3.0);

  setUpComplete = preferences.getBool("setUpComplete", false);

  preferences.end();
}

void initSensors() {
  pinMode(MOISTURE_SENSOR_1, INPUT);
  pinMode(MOISTURE_SENSOR_2, INPUT);
  pinMode(WATER_LEVEL,       INPUT);
  pinMode(TdsSensorPin,      INPUT);

  // ✅ Ultrasonic setup (was missing)
  pinMode(ULTRA_TRIG_PIN, OUTPUT);
  pinMode(ULTRA_ECHO_PIN, INPUT);      // don't pullup; echo is driven by sensor
  digitalWrite(ULTRA_TRIG_PIN, LOW);   // keep TRIG low when idle

  sensors.begin();
  ads.begin();
}

// Median filter function to smooth out ultrasonic distance readings
float getUltrasonicMedianDistanceCM() {
  const int N = 5;  // Number of readings for the median filter
  float readings[N];
  
  for (int i = 0; i < N; i++) {
    readings[i] = readUltrasonicDistanceCM();
    delay(20);  // small delay to prevent too fast readings
  }

  // Sort the readings (simple bubble sort for demonstration)
  for (int i = 0; i < N - 1; i++) {
    for (int j = 0; j < N - i - 1; j++) {
      if (readings[j] > readings[j + 1]) {
        float temp = readings[j];
        readings[j] = readings[j + 1];
        readings[j + 1] = temp;
      }
    }
  }

  // Return the median value (middle value in the sorted array)
  return readings[N / 2];  // median of the sorted array
}

// ------------------ Ultrasonic implementation ------------------
float readUltrasonicDistanceCM() {
  // Ensure a clean trigger (idle low)
  digitalWrite(ULTRA_TRIG_PIN, LOW);
  delayMicroseconds(3);

  // 10 µs trigger pulse
  digitalWrite(ULTRA_TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(ULTRA_TRIG_PIN, LOW);

  // Measure echo HIGH pulse width with timeout
  unsigned long duration = pulseIn(ULTRA_ECHO_PIN, HIGH, ULTRA_TIMEOUT_US);

  if (duration == 0) {
    // Timeout → no echo; wiring/voltage-level/angle issue likely
    Debug.println("[ULTRA] timeout (no echo)");
    // Return a far distance so level% computes to ~0
    return ULTRA_EMPTY_CM + 100.0f;
  }

  // HC-SR04 formula: distance(cm) = duration(µs) / 58.0
  float cm = duration / 58.0f;

  // Basic sanity clamp (HC-SR04 ~2–400 cm). Discard glitches.
  if (cm < 2.0f || cm > 400.0f) {
    Debug.println("[ULTRA] out-of-range: " + String(cm) + " cm");
    return ULTRA_EMPTY_CM + 100.0f;
  }

  return cm;
}

int distanceToLevelPercent(float cm) {
  // Map distance (empty..full) → 0..100%, then clamp
  // When cm == ULTRA_FULL_CM  → 100%
  // When cm == ULTRA_EMPTY_CM → 0%
  
  // Ensure that the cm value is within the range of valid values
  if (cm < ULTRA_FULL_CM) {
    return 100;  // 100% full
  }
  if (cm > ULTRA_EMPTY_CM) {
    return 0;    // 0% full
  }

  int percent = (int) roundf(
      (ULTRA_EMPTY_CM - cm) * 100.0f / (ULTRA_EMPTY_CM - ULTRA_FULL_CM)
  );
  return constrain(percent, 0, 100);  // Ensure percent stays between 0% and 100%
}

// ------------------ Reading Sensors and Uploading ------------------
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

  // 🔊 Ultrasonic readings — take several reads, use median for stability
  float dist_cm = getUltrasonicMedianDistanceCM();  // Use median filtered reading
  g_sensorData.ultra_distance_cm   = dist_cm;
  g_sensorData.ultra_level_percent = distanceToLevelPercent(dist_cm);

  // Optionally, fill TDS and pH values when available
  g_sensorData.tds_val = getTDSValue(); // Placeholder
  g_sensorData.ph_val  = getPHValue();  // Placeholder

  Debug.println("Sensor Readings:");
  Debug.println("Temperature 1: " + String(g_sensorData.temp_val_1));
  Debug.println("Temperature 2: " + String(g_sensorData.temp_val_2));
  Debug.println("Moisture 1: "   + String(g_sensorData.moist_percent_1));
  Debug.println("Moisture 2: "   + String(g_sensorData.moist_percent_2));
  Debug.println("Water Level: "  + String(g_sensorData.water_level));
  Debug.println("US Distance: "  + String(g_sensorData.ultra_distance_cm) + " cm");
  Debug.println("US Level: "     + String(g_sensorData.ultra_level_percent) + " %");
  Debug.println("TDS Value: "    + String(g_sensorData.tds_val) + " ppm");
  Debug.println("PH Value: "     + String(g_sensorData.ph_val));
}

int getMoistureVal(int PIN, int airVal, int waterVal){
  int rawVal  = analogRead(PIN);
  int percent = map(rawVal, waterVal, airVal, 100, 0);
  return constrain(percent, 0, 100);
}

int getWaterLevel(){
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

  static unsigned long printTimepoint = millis();
  if (millis() - printTimepoint > 800U) {
    printTimepoint = millis();
    for (int i = 0; i < SCOUNT; i++) analogBufferTemp[i] = analogBuffer[i];
    float averageVoltage = getMedianNum(analogBufferTemp, SCOUNT) * VREF / 4095.0;

    Serial.print("Average Voltage: ");
    Serial.println(averageVoltage);

    // Linear interpolation between two calibration points
    float V1 = 0.55;  // Voltage at 84 µS/cm
    float TDS1 = 84;  // µS/cm
    float V2 = 1.81;  // Voltage at 1413 µS/cm
    float TDS2 = 1413;

    float tdsValue = ((TDS2 - TDS1) / (V2 - V1)) * (averageVoltage - V1) + TDS1;
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
  const int NUM_SAMPLES = 10;     // Number of samples to average
  float totalVoltage = 0.0;

  // Take multiple readings for stability
  for (int i = 0; i < NUM_SAMPLES; i++) {
    int16_t adc0 = ads.readADC_SingleEnded(0);  // Read ADC value
    float voltage = ads.computeVolts(adc0);     // Convert to voltage
    totalVoltage += voltage;
    delay(50); // Small delay between samples (adjust if needed)
  }

  // Compute average voltage
  float avgVoltage = totalVoltage / NUM_SAMPLES;

  // Two-point calibration values (adjust to your sensor)
  float voltage_pH6_86 = 2.46;
  float voltage_pH9_18 = 2.20;
  float pH6_86 = 6.86;
  float pH9_18 = 9.18;

  // Compute slope (voltage difference per pH unit)
  float pH_step = (voltage_pH9_18 - voltage_pH6_86) / (pH9_18 - pH6_86);

  // Calculate pH based on average voltage
  float pH_value = (avgVoltage - voltage_pH6_86) / pH_step + pH6_86;

  return pH_value;
}
