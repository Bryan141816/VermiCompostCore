#ifndef CONFIG_H
#define CONFIG_H

// Device Info
#define DEVICE_ID   "1934"
#define DEVICE_NAME "Vermi_Compost_" DEVICE_ID
#define MDNS_HOST   "vermi" DEVICE_ID

// Firebase
#define FIREBASE_API_KEY "AIzaSyB9-EB5TZOP3JNg1dPt0466uFLe3xHQuM4"
#define FIREBASE_DB_URL "https://vermicompost-bd9f5-default-rtdb.asia-southeast1.firebasedatabase.app/"
#define EMAIL "dorisaligato2002@gmail.com"
#define PASSWORD "099996985858"

// Other Settings
#define UPLOAD_INTERVAL 30000 // ms
#define PUMP_DURATION 5000 // ms
#define PUMP_COOLDOWN 30000 // ms
#define DEBUG_MODE true


// WiFi Credentials will only be used for debug mode
#define USE_PREDEFINED_WIFI true
#define WIFI_SSID "Antopina"
#define WIFI_PASSWORD "AntopinaAlyza_042203"

//Disable features

#define DISABLE_FIREBASE false
#define DISABLE_WIFI_SERVER false
#define DISABLE_PUMP false

// Combined flags (will be used to make one conditions in code)
// DON't change anything here
#define DEBUG_DEFAULT_WIFI (USE_PREDEFINED_WIFI && DEBUG_MODE)
#define DEBUG_FIREBASE (DISABLE_FIREBASE && DEBUG_MODE)
#define DEBUG_WIFI_SERVER   (DISABLE_WIFI_SERVER && DEBUG_MODE)
#define DEBUG_PUMP     (DISABLE_PUMP && DEBUG_MODE)


#endif
