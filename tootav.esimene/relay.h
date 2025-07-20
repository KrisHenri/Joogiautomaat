#ifndef RELAY_H
#define RELAY_H

#include "M5UnitPbHub.h"

#define RELAY_CHANNEL 1
#define RELAY_INDEX1 0
#define RELAY_INDEX2 1

extern M5UnitPbHub pbhub;

void toggleRelay(int channel, int index, bool &state) {
    pbhub.digitalWrite(channel, index, state ? HIGH : LOW);
}

#endif
