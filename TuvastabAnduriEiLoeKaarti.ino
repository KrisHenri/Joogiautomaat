#include "RFID.h"

RFIDReader rfid;

void setup() {
    M5.begin();
    rfid.begin();
    Serial.begin(115200);
    Serial.println("RFID reader initialized");
}

void loop() {
    rfid.update();
    if (rfid.hasNewTag()) {
        Serial.println("New RFID Tag: " + rfid.getLastTag());
    }
    delay(50);
}
