#ifndef SENSOR_HANDLER_H
#define SENSOR_HANDLER_H

#include <Arduino.h>
#include "SensorsData.h"  // ✅ Include your struct header

extern SensorData g_sensorData;  // ✅ Global variable for all sensor values
extern bool setUpComplete;
extern int valAir1 ;
extern int valWater1 ;
extern int valAir2 ;
extern int valWater2 ;
// extern float Tankempty ;
// extern float TankFull ;

// ---- Ultrasonic calibration (distance from sensor to water surface)
extern float ULTRA_EMPTY_CM;   // distance when TANK is EMPTY (far surface)
extern float ULTRA_FULL_CM;    // distance when TANK is FULL (near surface)


void initSensors();
void readSensors();
int getMoistureVal(int pin, int valAir, int valWater);
int getWaterLevel();
float getTDSValue();
float getPHValue();
int getMedianNum(int bArray[], int len);

// Ultrasonic helpers
void  loadOrSetDefaults();
void  setUltrasonicCalibration(float empty_cm, float full_cm, bool save = true);
float readUltrasonicDistanceCM();         // raw distance read
int   distanceToLevelPercent(float cm);   // distance → %full using calibration


#endif
