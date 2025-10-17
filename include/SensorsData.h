#ifndef SENSORS_DATA_H
#define SENSORS_DATA_H

struct SensorData {
    float temp_val_1;
    float temp_val_2;
    int   moist_percent_1;
    int   moist_percent_2;
    int   avg_moisture; 
    float water_level;
    float tds_val;
    float ph_val;
    float ultra_distance_cm;    // measured distance from sensor to water (cm)
    int   ultra_level_percent; 
};

extern SensorData g_sensorData;  // Declare global variable

#endif
