#include "TouchController.h"

#define BUTTON_WIDTH 100
#define BUTTON_HEIGHT 50
#define BUTTON_MARGIN 20

TouchController::TouchController(Flashlight* flash, RelayController* relay) 
    : flashlight(flash), relayController(relay) {
    flashlightBtn = {BUTTON_MARGIN, BUTTON_MARGIN, BUTTON_WIDTH, BUTTON_HEIGHT};
    relay1Btn = {BUTTON_MARGIN, BUTTON_MARGIN*2 + BUTTON_HEIGHT, BUTTON_WIDTH, BUTTON_HEIGHT};
    relay2Btn = {BUTTON_MARGIN, BUTTON_MARGIN*3 + BUTTON_HEIGHT*2, BUTTON_WIDTH, BUTTON_HEIGHT};
}

void TouchController::init() {
    M5.Touch.begin();
    drawUI(false); // Initialize with PIR inactive
}

void TouchController::update() {
    handleTouch();
}

void TouchController::drawUI(bool pirState) {
    M5.Lcd.fillScreen(BLACK);
    
    // Flashlight button
    M5.Lcd.fillRect(flashlightBtn.x, flashlightBtn.y, flashlightBtn.w, flashlightBtn.h, 
                   flashlight->getState() ? GREEN : RED);
    M5.Lcd.setCursor(flashlightBtn.x + 10, flashlightBtn.y + 20);
    M5.Lcd.print("Flashlight");
    
    // Relay buttons
    M5.Lcd.fillRect(relay1Btn.x, relay1Btn.y, relay1Btn.w, relay1Btn.h, 
                   relayController->getRelay1() ? GREEN : RED);
    M5.Lcd.setCursor(relay1Btn.x + 30, relay1Btn.y + 20);
    M5.Lcd.print("Relay 1");
    
    M5.Lcd.fillRect(relay2Btn.x, relay2Btn.y, relay2Btn.w, relay2Btn.h, 
                   relayController->getRelay2() ? GREEN : RED);
    M5.Lcd.setCursor(relay2Btn.x + 30, relay2Btn.y + 20);
    M5.Lcd.print("Relay 2");
    
    // Status
    M5.Lcd.setCursor(BUTTON_MARGIN, M5.Lcd.height() - 30);
    M5.Lcd.printf("PIR: %s", pirState ? "ACTIVE" : "INACTIVE");
}

void TouchController::handleTouch() {
    TouchPoint_t touch = M5.Touch.getPressPoint();
    if (touch.x == -1 || touch.y == -1) return;
    
    unsigned long currentMillis = millis();
    if (currentMillis - lastButtonPressTime < buttonDebounceDelay) return;
    lastButtonPressTime = currentMillis;
    
    if (isTouched(flashlightBtn, touch)) {
        flashlight->set(!flashlight->getState());
        drawUI(false); // Pass false as we don't have PIR state here
    }
    else if (isTouched(relay1Btn, touch)) {
        relayController->setRelay1(!relayController->getRelay1());
        drawUI(false);
    }
    else if (isTouched(relay2Btn, touch)) {
        relayController->setRelay2(!relayController->getRelay2());
        drawUI(false);
    }
}

bool TouchController::isTouched(Rect_t area, TouchPoint_t touch) {
    return touch.x > area.x && touch.x < area.x + area.w &&
           touch.y > area.y && touch.y < area.y + area.h;
}