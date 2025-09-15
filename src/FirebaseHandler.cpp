
#define ENABLE_USER_AUTH 
#define ENABLE_DATABASE

#include "FirebaseHandler.h" 
#include "SensorsData.h" 
#include "Config.h" 
#include "SerialDebugger.h" 

// Provides the functions used in the examples. #include <ArduinoJson.h>
// ===== Firebase globals =====
FirebaseApp app;
RealtimeDatabase Database;
bool firebaseBusy = false;

// Two SSL clients & async clients (one for writes, one for stream)
WiFiClientSecure ssl_client1, ssl_client2;
using AsyncClient = AsyncClientClass;
AsyncClient async_client1(ssl_client1), async_client2(ssl_client2);

UserAuth *userAuth = nullptr;

// ===== Pump control state =====
static volatile bool g_pumpState = false;        // last known state from Firebase
static String g_pumpPath;                        // /VermiBoxes/<DEVICE_ID>/Control/isPump
static bool g_streamActive = false;

// Forward
static void processData(AsyncResult &aResult);
static void setPump(bool on);

// ===== Process all Firebase callbacks (writes + stream) =====
static void processData(AsyncResult &aResult) {
  if (!aResult.isResult()) return;

  if (aResult.isEvent()) {
    Firebase.printf("Event: %s | Msg: %s | Code: %d\n",
                    aResult.uid().c_str(),
                    aResult.eventLog().message().c_str(),
                    aResult.eventLog().code());
  }

  if (aResult.isDebug()) {
    Firebase.printf("Debug: %s | Msg: %s\n",
                    aResult.uid().c_str(),
                    aResult.debug().c_str());
  }

  if (aResult.isError()) {
    Firebase.printf("Error: %s | Msg: %s | Code: %d\n",
                    aResult.uid().c_str(),
                    aResult.error().message().c_str(),
                    aResult.error().code());
    // Failsafe for pump stream errors
    if (String(aResult.uid()) == "pumpStream") {
      setPump(false);
      // allow a restart later
      g_streamActive = false;
    }
  }

  if (!aResult.available()) {
    firebaseBusy = false;
    return;
  }

  // Convert to RTDB result (now that we know it's available)
  RealtimeDatabaseResult &RTDB = aResult.to<RealtimeDatabaseResult>();

  // If this is not a stream (e.g., a write callback), just log payload
  if (!RTDB.isStream()) {
    Firebase.printf("Task: %s | Payload: %s\n",
                    aResult.uid().c_str(), aResult.c_str());
    firebaseBusy = false;
    return;
  }

  // ---- Stream data path/value ----
  const bool isPumpStream = (String(aResult.uid()) == "pumpStream");
  const int  t = RTDB.type();  // 4=bool, 1=int, 6=JSON
  const String ev = RTDB.event();      // "put", "patch", ...
  const String path = RTDB.dataPath(); // e.g. "/"
  const String dataStr = RTDB.to<String>();

  Firebase.printf("[STREAM %s] event=%s path=%s type=%d data=%s\n",
                  aResult.uid().c_str(), ev.c_str(), path.c_str(), t, dataStr.c_str());

  if (isPumpStream) {
    if (t == 4 /* bool */ || t == 1 /* int */) {
      const bool pumpState = RTDB.to<bool>();
      setPump(pumpState);
      Serial.printf("[pumpStream] isPump -> %s\n", pumpState ? "ON" : "OFF");
    } else if (t == 6 /* JSON */) {
      // If your stream path is a parent, you may get a JSON snapshot
      // Example: { "isPump": true }
      const bool foundTrue  = (dataStr.indexOf("\"isPump\"") >= 0) && (dataStr.indexOf("true")  >= 0);
      const bool foundFalse = (dataStr.indexOf("\"isPump\"") >= 0) && (dataStr.indexOf("false") >= 0);
      if (foundTrue || foundFalse) {
        setPump(foundTrue);
        Serial.printf("[pumpStream] JSON isPump -> %s\n", foundTrue ? "ON" : "OFF");
      }
    }

    // Optional: if auth revoked/cancel, mark inactive so loop can restart it
    if (ev == "cancel" || ev == "auth_revoked") {
      g_streamActive = false;
    }
  }

  firebaseBusy = false;
}


// ===== Init Firebase (auth + DB URL) =====
void initFirebase(const char* apiKey,
                  const char* email,
                  const char* password,
                  const char* dbUrl) {
  // Secure clients (you can replace setInsecure() with proper cert later)
  ssl_client1.setInsecure();
  ssl_client2.setInsecure();

  // Reasonable timeouts
   ssl_client1.setTimeout(1000);
   ssl_client1.setHandshakeTimeout(5);
   ssl_client2.setTimeout(1000);
   ssl_client2.setHandshakeTimeout(5);

  // Auth
  userAuth = new UserAuth(apiKey, email, password);

  // App + RTDB
  initializeApp(async_client1, app, getAuth(*userAuth), processData, "authTask");
  app.getApp<RealtimeDatabase>(Database);
  Database.url(dbUrl);

  // Build the pump path using your DEVICE_ID from Config.h
  g_pumpPath = String("Control/") + String(DEVICE_ID) + "/isPump";

  // Prepare pump pin
  //pinMode(PUMP_PIN, OUTPUT);
  setPump(false); // default OFF on boot


  // Start the stream right away
 
}

// ===== Keep Firebase alive in loop() =====
void firebaseLoop() {
  app.loop();
   startPumpListener();
}

// ===== Explicitly start the isPump stream =====
void startPumpListener() {
  if (!app.ready()) return;
  if (g_streamActive) return;

  async_client2.setSSEFilters("get,put,patch,keep-alive,cancel,auth_revoked");
  // Use SSE/streaming mode so changes are pushed to the device
  Serial.println(g_pumpPath.c_str());
  Database.get(async_client2,
               g_pumpPath.c_str(),
               processData,
               true /* SSE streaming */,
               "pumpStream");
  g_streamActive = true;
  Serial.printf("Started pump listener at: %s\n", g_pumpPath.c_str());
}

// ===== Optional: stop the stream =====
void stopPumpListener() {
  // Note: FirebaseClient closes streams automatically when client goes out of scope
  // If you manage multiple streams, you can track/close here when the lib exposes it.
  g_streamActive = false;
}

// ===== Upload: Real-time data =====
void uploadDataToFirebase(const SensorData &data) {
  if (!app.ready() || firebaseBusy) return;
  firebaseBusy = true;
  //Debug.println("Firebase: Sending realtime data");

  String base = "/RealTimeData/" + String(DEVICE_ID) + "/";
  Database.set<float>(async_client1, (base + "temp0").c_str(), data.temp_val_1, processData, "RTDB_Float");
  Database.set<float>(async_client1, (base + "temp1").c_str(), data.temp_val_2, processData, "RTDB_Float");
  Database.set<int>  (async_client1, (base + "moisture1").c_str(), data.moist_percent_1, processData, "RTDB_Int");
  Database.set<int>  (async_client1, (base + "moisture2").c_str(), data.moist_percent_2, processData, "RTDB_Int");
  Database.set<float>(async_client1, (base + "water_level").c_str(), data.water_level, processData, "RTDB_Float");
  Database.set<float>(async_client1, (base + "tds_val").c_str(), data.tds_val, processData, "RTDB_Float");
  Database.set<float>(async_client1, (base + "ph_level").c_str(), data.ph_val, processData, "RTDB_Float");
  Database.set<float>(async_client1, (base + "ultra_distance_cm").c_str(), data.ultra_distance_cm, processData, "RTDB_Float");
  Database.set<float>(async_client1, (base + "ultra_level_percent").c_str(), data.ultra_level_percent, processData, "RTDB_Float");
}

// ===== Upload: Records data (by date) =====
// ===== Upload: Records data (by date) =====
void uploadRecordDataToFirebase(const String &date, const SensorData &data) {
  if (!app.ready() || firebaseBusy) return;
  firebaseBusy = true;
  //Debug.println("Firebase: Sending database value");

  String base = "/RecordsData/" + String(DEVICE_ID) + "/" + date + "/";
  Database.set<float>(async_client1, (base + "temp0").c_str(), data.temp_val_1, processData, "RTDB_Float");
  Database.set<float>(async_client1, (base + "temp1").c_str(), data.temp_val_2, processData, "RTDB_Float");
  Database.set<int>  (async_client1, (base + "moisture1").c_str(), data.moist_percent_1, processData, "RTDB_Int");
  Database.set<int>  (async_client1, (base + "moisture2").c_str(), data.moist_percent_2, processData, "RTDB_Int");
  Database.set<float>(async_client1, (base + "water_level").c_str(), data.water_level, processData, "RTDB_Float");
  Database.set<float>(async_client1, (base + "tds_val").c_str(), data.tds_val, processData, "RTDB_Float");
  Database.set<float>(async_client1, (base + "ph_val").c_str(), data.ph_val, processData, "RTDB_Float");
  Database.set<float>(async_client1, (base + "ultra_distance_cm").c_str(), data.ultra_distance_cm, processData, "RTDB_Float");
  Database.set<float>(async_client1, (base + "ultra_level_percent").c_str(), data.ultra_level_percent, processData, "RTDB_Float");
}


// ===== Helpers =====
static void setPump(bool on) {
  g_pumpState = on;
  //digitalWrite(PUMP_PIN, on ? HIGH : LOW);
  Debug.println("Pumping "+ String(g_pumpState));
}

bool getPumpState() {
  return g_pumpState;
}
