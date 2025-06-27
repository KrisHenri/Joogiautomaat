#include <M5Core2.h>
#include <WiFi.h>
#include <stdint.h>
#include "TouchController.h"

// Color definitions
#define BLACK 0x0000
#define WHITE 0xFFFF
#define RED 0xF800
#define GREEN 0x07E0
#define BLUE 0x001F
#define DARKGREY 0x7BEF

#define BUTTON_WIDTH 120
#define BUTTON_HEIGHT 60
#define BUTTON_MARGIN 20
#define SETTINGS_BTN_SIZE 40
#define PROGRESS_BAR_HEIGHT 20

// Wrench icon (16x16 pixels)
const uint8_t wrenchIcon[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

TouchController::TouchController(Flashlight* flash, RelayController* relay) 
    : flashlight(flash), relayController(relay) {
    // Calculate button positions for main UI
    int centerX = M5.Lcd.width() / 2;
    int centerY = M5.Lcd.height() / 2;
    
    // Position buttons in the center of the screen
    relay1Btn = {
        centerX - BUTTON_WIDTH - BUTTON_MARGIN/2,  // Left button
        centerY - BUTTON_HEIGHT/2,
        BUTTON_WIDTH,
        BUTTON_HEIGHT
    };
    
    relay2Btn = {
        centerX + BUTTON_MARGIN/2,  // Right button
        centerY - BUTTON_HEIGHT/2,
        BUTTON_WIDTH,
        BUTTON_HEIGHT
    };
    
    // Settings button in top-right corner
    settingsBtn = {
        M5.Lcd.width() - SETTINGS_BTN_SIZE - BUTTON_MARGIN,
        BUTTON_MARGIN,
        SETTINGS_BTN_SIZE,
        SETTINGS_BTN_SIZE
    };

    // Progress bar at the bottom
    progressBar = {
        BUTTON_MARGIN,
        M5.Lcd.height() - PROGRESS_BAR_HEIGHT - BUTTON_MARGIN,
        M5.Lcd.width() - (2 * BUTTON_MARGIN),
        PROGRESS_BAR_HEIGHT
    };
}

void TouchController::init() {
    M5.Touch.begin();
    drawMainUI(false); // Initialize with PIR inactive
}

void TouchController::update() {
    handleTouch();
}

void TouchController::drawUI(bool pirState) {
    if (inSettings) {
        drawSettingsUI(pirState);
    } else {
        drawMainUI(pirState);
    }
}

void TouchController::drawMainUI(bool pirState) {
    M5.Lcd.fillScreen(BLACK);
    
    // Draw relay buttons
    M5.Lcd.fillRect(relay1Btn.x, relay1Btn.y, relay1Btn.w, relay1Btn.h, 
                   relayController->getRelay1() ? GREEN : RED);
    M5.Lcd.setCursor(relay1Btn.x + 20, relay1Btn.y + 25);
    M5.Lcd.setTextSize(2);
    M5.Lcd.print("Jook 1");
    
    M5.Lcd.fillRect(relay2Btn.x, relay2Btn.y, relay2Btn.w, relay2Btn.h, 
                   relayController->getRelay2() ? GREEN : RED);
    M5.Lcd.setCursor(relay2Btn.x + 20, relay2Btn.y + 25);
    M5.Lcd.print("Jook 2");
    
    // Draw settings button
    M5.Lcd.fillRect(settingsBtn.x, settingsBtn.y, settingsBtn.w, settingsBtn.h, BLUE);
    M5.Lcd.setCursor(settingsBtn.x + 10, settingsBtn.y + 15);
    M5.Lcd.setTextSize(1);
    M5.Lcd.print("SET");
}

void TouchController::drawSettingsUI(bool pirState) {
    M5.Lcd.fillScreen(BLACK);
    
    // Draw settings title
    M5.Lcd.setCursor(BUTTON_MARGIN, BUTTON_MARGIN);
    M5.Lcd.setTextSize(2);
    M5.Lcd.print("Settings");
    
    // Draw IP address
    M5.Lcd.setCursor(BUTTON_MARGIN, BUTTON_MARGIN + 40);
    M5.Lcd.setTextSize(1);
    M5.Lcd.print("IP Address:");
    M5.Lcd.setCursor(BUTTON_MARGIN, BUTTON_MARGIN + 60);
    M5.Lcd.setTextSize(2);
    M5.Lcd.print(WiFi.localIP().toString());
    
    // Draw PIR status
    M5.Lcd.setCursor(BUTTON_MARGIN, BUTTON_MARGIN + 100);
    M5.Lcd.setTextSize(1);
    M5.Lcd.print("PIR Status:");
    M5.Lcd.setCursor(BUTTON_MARGIN, BUTTON_MARGIN + 120);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setTextColor(pirState ? GREEN : RED);
    M5.Lcd.printf("%s", pirState ? "ACTIVE" : "INACTIVE");
    M5.Lcd.setTextColor(WHITE);
    
    // Draw back button
    M5.Lcd.fillRect(BUTTON_MARGIN, M5.Lcd.height() - BUTTON_HEIGHT - BUTTON_MARGIN,
                   BUTTON_WIDTH, BUTTON_HEIGHT, BLUE);
    M5.Lcd.setCursor(BUTTON_MARGIN + 20, M5.Lcd.height() - BUTTON_HEIGHT - BUTTON_MARGIN + 25);
    M5.Lcd.setTextSize(2);
    M5.Lcd.print("Back");
}

void TouchController::handleTouch() {
    TouchPoint_t touch = M5.Touch.getPressPoint();
    if (touch.x == -1 || touch.y == -1) return;
    
    unsigned long currentMillis = millis();
    if (currentMillis - lastButtonPressTime < buttonDebounceDelay) return;
    lastButtonPressTime = currentMillis;
    
    if (inSettings) {
        // Handle settings screen touches
        if (isTouched({BUTTON_MARGIN, M5.Lcd.height() - BUTTON_HEIGHT - BUTTON_MARGIN,
                      BUTTON_WIDTH, BUTTON_HEIGHT}, touch)) {
            inSettings = false;
            drawMainUI(false);
        }
    } else {
        // Handle main screen touches
        if (isTouched(relay1Btn, touch)) {
            relayController->setRelay1(!relayController->getRelay1());
            drawMainUI(false);
        }
        else if (isTouched(relay2Btn, touch)) {
            relayController->setRelay2(!relayController->getRelay2());
            drawMainUI(false);
        }
        else if (isTouched(settingsBtn, touch)) {
            inSettings = true;
            drawSettingsUI(false);
        }
    }
}

bool TouchController::isTouched(Rect_t area, TouchPoint_t touch) {
    return touch.x > area.x && touch.x < area.x + area.w &&
           touch.y > area.y && touch.y < area.y + area.h;
}