#pragma once
#include <Arduino.h>

// --- Required API ---
void initPump();                 // call in setup()
void setPump(bool isPump);       // ON = true, OFF = false

