#ifndef TOUCHCONTROLLER_H
#define TOUCHCONTROLLER_H

#include <M5Core2.h>
#include <WiFi.h>
#include "Flashlight.h"
#include "RelayController.h"

// Define TouchPoint_t if not already defined by M5Core2
#ifndef TouchPoint_t
typedef struct {
    int x;
    int y;
} TouchPoint_t;
#endif

// Color definitions
#define BLACK 0x0000
#define WHITE 0xFFFF
#define RED 0xF800
#define GREEN 0x07E0
#define BLUE 0x001F
#define DARKGREY 0x7BEF

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
    
    Rect_t relay1Btn;
    Rect_t relay2Btn;
    Rect_t settingsBtn;
    Rect_t progressBar;
    
    bool inSettings = false;  // Track if we're in settings mode
    bool isPouring = false;   // Track if drink is being poured
    unsigned long pourStartTime = 0;
    float targetVolume = 0.25f;  // Target volume in liters (250ml)
    
    unsigned long lastButtonPressTime = 0;
    const unsigned long buttonDebounceDelay = 50;
    
    void handleTouch();
    bool isTouched(Rect_t area, TouchPoint_t touch);
    void drawMainUI(bool pirState);
    void drawSettingsUI(bool pirState);
    void drawProgressBar();
    void startPouring();
    void stopPouring();
};

#endif