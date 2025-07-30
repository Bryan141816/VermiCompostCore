
#include "SerialDebugger.h"

Debugger Debug; // Define global instance

void Debugger::begin(unsigned long baud) {
    Serial.begin(baud);
    if (DEBUG_MODE) {
        while (!Serial) { delay(10); }
        Serial.println("[DEBUG] Serial initialized");
    }
}

void Debugger::print(const String &msg) {
    if (DEBUG_MODE) Serial.print(msg);
}

void Debugger::println(const String &msg) {
    if (DEBUG_MODE) Serial.println(msg);
}

void Debugger::printf(const char *format, ...) {
    if (DEBUG_MODE) {
        char buffer[128];
        va_list args;
        va_start(args, format);
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);
        Serial.print(buffer);
    }
}
