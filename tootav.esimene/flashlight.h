#ifndef FLASHLIGHT_H
#define FLASHLIGHT_H

#include "M5UnitPbHub.h"

#define FLASHLIGHT_CHANNEL 0
#define FLASHLIGHT_INDEX 1

extern M5UnitPbHub pbhub;

void unit_flash_init() {
    pbhub.digitalWrite(FLASHLIGHT_CHANNEL, FLASHLIGHT_INDEX, LOW);
}

void unit_flash_set_brightness(bool state) {
    pbhub.digitalWrite(FLASHLIGHT_CHANNEL, FLASHLIGHT_INDEX, state ? HIGH : LOW);
}

#endif
