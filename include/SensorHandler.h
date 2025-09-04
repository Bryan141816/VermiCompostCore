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
void initSensors();
void readSensors();
int getMoistureVal(int pin, int valAir, int valWater);
int getWaterLevel();
float getTDSValue();
float getPHValue();
int getMedianNum(int bArray[], int len);

#endif
