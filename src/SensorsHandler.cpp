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
#define WATER_LEVEL 34
#define TdsSensorPin 35

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
// ✅ Define the global variable
SensorData g_sensorData = {0};

// 🔊 Ultrasonic pins
#define ULTRA_TRIG_PIN     5
#define ULTRA_ECHO_PIN     18
#define ULTRA_TIMEOUT_US   30000UL 


float ULTRA_EMPTY_CM = 14.0f;  // distance at EMPTY (farther)
float ULTRA_FULL_CM  = 4.0f;

void loadOrSetDefaults() {
  preferences.begin("config", true);

  // Load or initialize integer values
  valAir1 = preferences.getInt("valAir1", 3018);

  valWater1 = preferences.getInt("valWater1", 1710);

  valAir2 = preferences.getInt("valAir2", 3018);

  valWater2 = preferences.getInt("valWater2", 1710);

//   Tankempty = preferences.getFloat("Tankempty", 10.0);

//   TankFull = preferences.getFloat("TankFull", 3.0);
  
  setUpComplete = preferences.getBool("setUpComplete", false);


  preferences.end();
}
void initSensors() {
    pinMode(MOISTURE_SENSOR_1, INPUT);
    pinMode(MOISTURE_SENSOR_2, INPUT);
    pinMode(WATER_LEVEL, INPUT);
    pinMode(TdsSensorPin, INPUT);

    sensors.begin();
    ads.begin();

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
  Debug.println("US Level:   " + String(g_sensorData.ultra_level_percent) + " %");
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

    // Print the voltage to the serial monitor
    Serial.print("Average Voltage: ");
    Serial.println(averageVoltage);

    // Calculate TDS in µS/cm based on voltage
    // Linear interpolation: TDS = (TDS2 - TDS1) / (V2 - V1) * (V - V1) + TDS1
    float V1 = 0.55; // Voltage at 84 µS/cm
    float TDS1 = 84; // TDS at 0.55V
    float V2 = 1.81; // Voltage at 1413 µS/cm
    float TDS2 = 1413; // TDS at 1.81V

    // Interpolation formula
    float tdsValue = ((TDS2 - TDS1) / (V2 - V1)) * (averageVoltage - V1) + TDS1;

    // Print the calculated TDS value to the serial monitor
    //Serial.print("TDS in µS/cm: ");
    //Serial.println(tdsValue);

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
  int16_t adc0 = ads.readADC_SingleEnded(0);  // Read the ADC value
  float voltage = ads.computeVolts(adc0);    // Convert ADC to voltage

  // Debug.println(String(voltage) + " V");

  // Define reference voltages and pH values
  float voltage_pH6_86 = 2.46;  // Voltage corresponding to pH 6.86
  float voltage_pH9_18 = 2.20;  // Voltage corresponding to pH 9.18
  float pH6_86 = 6.86;         // pH value for the first reference
  float pH9_18 = 9.18;         // pH value for the second reference

  // Calculate the slope (pH step) based on the two reference points
  float pH_step = (voltage_pH9_18 - voltage_pH6_86) / (pH9_18 - pH6_86);
  
  // Calculate pH value based on the voltage reading
  float pH_value = (voltage - voltage_pH6_86) / pH_step + pH6_86;

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
  Serial.println(String(duration) +" echo");
  if (duration == 0) {
    // timeout -> no echo; return a large number so % becomes 0
    return ULTRA_EMPTY_CM + 100.0f;
  }
  // HC-SR04: distance (cm) = duration(µs) / 58.0
  float cm = duration / 58.0f;
  return cm;

  //test
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
