#include "PumpHandler.h"
#include "Config.h"   // optional: define PUMP_RELAY and RELAY_ACTIVE_LOW here

#define PUMP_RELAY 25


#define PUMP_ON_LEVEL  HIGH
#define PUMP_OFF_LEVEL LOW


void initPump() {
  pinMode(PUMP_RELAY, OUTPUT);
  digitalWrite(PUMP_RELAY, PUMP_OFF_LEVEL);  // start OFF
}

void setPump(bool isPump) {

  digitalWrite(PUMP_RELAY, isPump ? PUMP_ON_LEVEL : PUMP_OFF_LEVEL);
}
