#ifndef SENSORS_DATA_H
#define SENSORS_DATA_H

struct SensorData {
    float temp_val_1;
    float temp_val_2;
    int   moist_percent_1;
    int   moist_percent_2;
    float water_level;
    float tds_val;
    float ph_val;
};

extern SensorData g_sensorData;  // Declare global variable

#endif