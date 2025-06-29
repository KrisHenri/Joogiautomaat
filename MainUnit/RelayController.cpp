#include "RelayController.h"

RelayController::RelayController(M5UnitPbHub* hub) : pbhub(hub) {}

void RelayController::setRelay1(bool on) {
    relay1 = on;
    pbhub->digitalWrite(RELAY_CHANNEL, RELAY_INDEX1, on ? HIGH : LOW);
}

void RelayController::setRelay2(bool on) {
    relay2 = on;
    pbhub->digitalWrite(RELAY_CHANNEL, RELAY_INDEX2, on ? HIGH : LOW);
}

bool RelayController::getRelay1() const {
    return relay1;
}

bool RelayController::getRelay2() const {
    return relay2;
}