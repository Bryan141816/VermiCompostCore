#ifndef DEBUG_H
#define DEBUG_H

#include <Arduino.h>
#include "Config.h"

class Debugger {
public:
    void begin(unsigned long baud);
    void print(const String &msg);
    void println(const String &msg);
    void printf(const char *format, ...);
};

// Declare global instance
extern Debugger Debug;

#endif
