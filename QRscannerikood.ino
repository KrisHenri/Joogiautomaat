#include <M5Unified.h>
#include <M5GFX.h>
#include "M5UnitQRCode.h"

M5Canvas canvas(&M5.Display);

// ⬇️ Muudame aadressi vastavalt QR mooduli tagaküljele
#define QR_UNIT_I2C_ADDR 0x21

// Kasutame Wire ja määrame GPIO32 (SDA), GPIO33 (SCL)
M5UnitQRCodeI2C qrcode;

void setup() {
    M5.begin();

    canvas.setColorDepth(1);  // mono color
    canvas.createSprite(M5.Display.width(), M5.Display.height());
    canvas.setTextSize((float)canvas.width() / 160);
    canvas.setTextScroll(true);

    while (!qrcode.begin(&Wire, QR_UNIT_I2C_ADDR, 32, 33, 100000U)) {
        canvas.println("Unit QRCode I2C Init Fail");
        Serial.println("Unit QRCode I2C Init Fail");
        canvas.pushSprite(0, 0);
        delay(1000);
    }

    canvas.println("Unit QRCode I2C Init Success");
    Serial.println("Unit QRCode I2C Init Success");

    canvas.println("Auto Scan Mode");
    canvas.pushSprite(0, 0);
    qrcode.setTriggerMode(AUTO_SCAN_MODE);
}

void loop() {
    if (qrcode.getDecodeReadyStatus() == 1) {
        uint8_t buffer[512] = {0};
        uint16_t length     = qrcode.getDecodeLength();
        Serial.printf("len:%d\r\n", length);
        qrcode.getDecodeData(buffer, length);

        Serial.printf("decode data:");
        for (int i = 0; i < length; i++) {
            Serial.printf("%c", buffer[i]);
            canvas.printf("%c", buffer[i]);
        }
        Serial.println();
        canvas.println();
        canvas.pushSprite(0, 0);
    }
}
