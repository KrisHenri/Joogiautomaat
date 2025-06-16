#include "Flashlight.h"

Flashlight::Flashlight(M5UnitPbHub* hub) : pbhub(hub) {}

void Flashlight::init() {
    pbhub->digitalWrite(FLASHLIGHT_CHANNEL, FLASHLIGHT_INDEX, LOW);
}

void Flashlight::set(bool on) {
    state = on;
    pbhub->digitalWrite(FLASHLIGHT_CHANNEL, FLASHLIGHT_INDEX, on ? HIGH : LOW);
}

bool Flashlight::getState() const {
    return state;
}
