#ifndef FIREBASE_HANDLER_H
#define FIREBASE_HANDLER_H

#define ENABLE_USER_AUTH
#define ENABLE_DATABASE

#include <WiFiClientSecure.h>
#include <FirebaseClient.h>
#include "SensorsData.h"

// ===== Firebase globals =====
extern FirebaseApp app;
extern RealtimeDatabase Database;
extern bool firebaseBusy;

// ===== Firebase core functions =====
void initFirebase(const char* apiKey,
                  const char* email,
                  const char* password,
                  const char* dbUrl);

void firebaseLoop();

void uploadDataToFirebase(const SensorData &data);
void uploadRecordDataToFirebase(const String &date, const SensorData &data);

// ===== Pump control functions =====
void startPumpListener();
void stopPumpListener();
bool getPumpState();

#endif // FIREBASE_HANDLER_H
