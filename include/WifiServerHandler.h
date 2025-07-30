#pragma once
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>

extern WebServer server;

void setupWiFiAndServer();
void loopWiFiAndServer();
