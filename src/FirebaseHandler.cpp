#include "FirebaseHandler.h"
#include "SensorsData.h"  // ✅ Include sensor struct
#include "Config.h"
#include "SerialDebugger.h"
// Firebase global variables
FirebaseApp app;
RealtimeDatabase Database;
bool firebaseBusy = false;

WiFiClientSecure ssl_client1, ssl_client2;

using AsyncClient = AsyncClientClass;
AsyncClient async_client1(ssl_client1), async_client2(ssl_client2);

UserAuth *userAuth = nullptr;

// ✅ Function to process Firebase results
void processData(AsyncResult &aResult) {
    if (!aResult.isResult()) return;

    if (aResult.isEvent())
        Firebase.printf("Event: %s | Msg: %s | Code: %d\n",
                        aResult.uid().c_str(),
                        aResult.eventLog().message().c_str(),
                        aResult.eventLog().code());

    if (aResult.isDebug())
        Firebase.printf("Debug: %s | Msg: %s\n", aResult.uid().c_str(), aResult.debug().c_str());

    if (aResult.isError())
        Firebase.printf("Error: %s | Msg: %s | Code: %d\n",
                        aResult.uid().c_str(),
                        aResult.error().message().c_str(),
                        aResult.error().code());

    if (aResult.available())
        Firebase.printf("Task: %s | Payload: %s\n", aResult.uid().c_str(), aResult.c_str());

    firebaseBusy = false;
}

void initFirebase(const char* apiKey, const char* email, const char* password, const char* dbUrl) {
    ssl_client1.setInsecure();
    ssl_client2.setInsecure();

    ssl_client1.setTimeout(1000);
    ssl_client1.setHandshakeTimeout(5);
    ssl_client2.setTimeout(1000);
    ssl_client2.setHandshakeTimeout(5);

    userAuth = new UserAuth(apiKey, email, password);

    initializeApp(async_client1, app, getAuth(*userAuth), processData, "authTask");
    app.getApp<RealtimeDatabase>(Database);
    Database.url(dbUrl);
}

void firebaseLoop() {
    app.loop();
}

// ✅ Upload Real-Time Data using SensorData
void uploadDataToFirebase(const SensorData &data) {
    if (!app.ready() || firebaseBusy) return;
    firebaseBusy = true;
    Debug.println("Firebase: Sending realtime data");
    Database.set<float>(async_client1, ("/RealTimeData/" + String(DEVICE_ID) + "/temp0").c_str(), data.temp_val_1, processData, "RTDB_Float");
    Database.set<float>(async_client1, ("/RealTimeData/" + String(DEVICE_ID) + "/temp1").c_str(), data.temp_val_2, processData, "RTDB_Float");
    Database.set<int>(async_client1, ("/RealTimeData/" + String(DEVICE_ID) + "/moisture1").c_str(), data.moist_percent_1, processData, "RTDB_Int");
    Database.set<int>(async_client1, ("/RealTimeData/" + String(DEVICE_ID) + "/moisture2").c_str(), data.moist_percent_2, processData, "RTDB_Int");
    Database.set<float>(async_client1, ("/RealTimeData/" + String(DEVICE_ID) + "/water_level").c_str(), data.water_level, processData, "RTDB_Float");
}

// ✅ Upload Record Data using SensorData
void uploadRecordDataToFirebase(const String &date, const SensorData &data) {
    if (!app.ready() || firebaseBusy) return;
    firebaseBusy = true;
    Debug.println("Firebase: Sending database value");
    String basePath = "/RecordsData/"+ String(DEVICE_ID) +"/" + date + "/";
    Database.set<float>(async_client2, (basePath + "temp0").c_str(), data.temp_val_1, processData, "RTDB_Float");
    Database.set<float>(async_client2, (basePath + "temp1").c_str(), data.temp_val_2, processData, "RTDB_Float");
    Database.set<int>(async_client2, (basePath + "moisture1").c_str(), data.moist_percent_1, processData, "RTDB_Int");
    Database.set<int>(async_client2, (basePath + "moisture2").c_str(), data.moist_percent_2, processData, "RTDB_Int");
    Database.set<float>(async_client2, (basePath + "water_level").c_str(), data.water_level, processData, "RTDB_Float");
}
