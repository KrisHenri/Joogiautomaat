#ifndef SCREEN_H
#define SCREEN_H

#include <M5Core2.h>

void initScreen() {
    M5.Lcd.setTextSize(2);
    M5.Lcd.setTextColor(WHITE);
    M5.Lcd.setCursor(0, 0);
    M5.Lcd.clear(BLACK);
}

void drawButtons(bool flashlightState, bool relay1State, bool relay2State, unsigned long pirTimeout, bool pirState) {
    M5.Lcd.setTextDatum(CC_DATUM);

    // Flashlight button
    M5.Lcd.fillRoundRect(30, 150, 50, 100, 10, flashlightState ? RED : GREEN);
    M5.Lcd.drawRoundRect(30, 150, 50, 100, 10, WHITE);
    M5.Lcd.drawCentreString(flashlightState ? "ON" : "OFF", 55, 200, 2);

    // Relay1 button
    M5.Lcd.fillRoundRect(120, 150, 80, 100, 10, relay1State ? RED : GREEN);
    M5.Lcd.drawRoundRect(120, 150, 80, 100, 10, WHITE);
    M5.Lcd.drawCentreString(relay1State ? "ON" : "OFF", 160, 200, 2);

    // Relay2 button
    M5.Lcd.fillRoundRect(230, 150, 40, 100, 10, relay2State ? RED : GREEN);
    M5.Lcd.drawRoundRect(230, 150, 40, 100, 10, WHITE);
    M5.Lcd.drawCentreString(relay2State ? "ON" : "OFF", 250, 200, 2);

    // PIR state
    M5.Lcd.drawString("PIR: ", 0, 0, 2);
    M5.Lcd.drawString(pirState ? "ON" : "OFF", 50, 0, 2);

    // PIR timeout
    M5.Lcd.drawString("PIR Timeout: ", 0, 20, 2);
    M5.Lcd.drawString(String(pirTimeout) + " ms", 150, 20, 2);
}

#endif // SCREEN_H
