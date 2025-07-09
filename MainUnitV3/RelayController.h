#ifndef RELAYCONTROLLER_H
#define RELAYCONTROLLER_H

#include "M5UnitPbHub.h"

#define RELAY_CHANNEL 1
#define RELAY_INDEX1 0
#define RELAY_INDEX2 1

class RelayController {
public:
    RelayController(M5UnitPbHub* hub);
    void setRelay1(bool on);
    void setRelay2(bool on);
    bool getRelay1() const;
    bool getRelay2() const;
    bool relay1 = false;
    bool relay2 = false;

private:
    M5UnitPbHub* pbhub;
};

#endif