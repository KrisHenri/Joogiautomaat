#ifndef FLASHLIGHT_H
#define FLASHLIGHT_H

#include "M5UnitPbHub.h"

#define FLASHLIGHT_CHANNEL 0
#define FLASHLIGHT_INDEX 1

class Flashlight {
public:
    Flashlight(M5UnitPbHub* hub);
    void init();
    void set(bool on);
    bool getState() const;
    bool state = false;

private:
    M5UnitPbHub* pbhub;
};

#endif
