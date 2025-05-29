#ifndef TOUCHCONTROLLER_H
#define TOUCHCONTROLLER_H

#include <M5Core2.h>
#include "Flashlight.h"
#include "RelayController.h"

class TouchController {
public:
    TouchController(Flashlight* flash, RelayController* relay);
    void init();
    void update();
    void drawUI(bool pirState);

private:
    Flashlight* flashlight;
    RelayController* relayController;
    
    typedef struct {
        int x;
        int y;
        int w;
        int h;
    } Rect_t;
    
    Rect_t flashlightBtn;
    Rect_t relay1Btn;
    Rect_t relay2Btn;
    
    unsigned long lastButtonPressTime = 0;
    const unsigned long buttonDebounceDelay = 50;
    
    void handleTouch();
    bool isTouched(Rect_t area, TouchPoint_t touch);
};

#endif