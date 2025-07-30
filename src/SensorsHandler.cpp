#include "SensorHandler.h"
#include <OneWire.h>
#include <DallasTemperature.h>
#include "Globals.h"
#include <Adafruit_ADS1X15.h>

Preferences preferences;
// DS18B20 setup
#define ONE_WIRE_BUS 13
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

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
bool setUpComplete = false;
// ✅ Define the global variable
SensorData g_sensorData = {0};



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

    DeviceAddress tempDeviceAddress;
    if (sensors.getAddress(tempDeviceAddress, 0))
        g_sensorData.temp_val_1 = sensors.getTempC(tempDeviceAddress);
    if (sensors.getAddress(tempDeviceAddress, 1))
        g_sensorData.temp_val_2 = sensors.getTempC(tempDeviceAddress);

    g_sensorData.moist_percent_1 = getMoistureVal(MOISTURE_SENSOR_1, valAir1, valWater1);
    g_sensorData.moist_percent_2 = getMoistureVal(MOISTURE_SENSOR_2, valAir2, valWater2);



    g_sensorData.water_level = getWaterLevel();

    // Optionally, fill TDS and pH values when available
    g_sensorData.tds_val = getTDSValue(); // Placeholder
    g_sensorData.ph_val  = getPHValue(); // Placeholder
}
int getMoistureVal(int PIN, int airVal, int waterVal){
    int rawVal = analogRead(PIN);
    int percent = map(rawVal, airVal, waterVal, 100, 0);
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

  float voltage_pH4 = 3.01;
  float voltage_pH6_86 = 2.60;
  float pH4 = 4.01;
  float pH6_86 = 6.86;

  float pH_step = (voltage_pH6_86 - voltage_pH4) / (pH6_86 - pH4);
  float pH_value = (voltage - voltage_pH4) / pH_step + pH4;

  return pH_value;
}